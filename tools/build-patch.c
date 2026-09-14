/* SPDX-License-Identifier: MIT
 * Link the assembled extension into the original driver and generate the patch code.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <imagehlp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/sha256.h"

typedef struct { BYTE *data; size_t size; } Buffer;
typedef struct { const char *name; DWORD address; } Symbol;
typedef struct { DWORD offset,length; const char *name; int call; } Hook;
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
static const BYTE source_hash[32]={
    0x34,0x1b,0x15,0x06,0x9f,0xc9,0x18,0x79,0xbd,0xdc,0xe5,0xd0,0x25,0x8d,0xea,0x08,
    0xee,0xdf,0xc1,0x85,0x5e,0x67,0xff,0x52,0xd7,0x70,0xc1,0x67,0x02,0xc8,0x97,0x81};
static const Symbol externs[]={
    {"wdf_globals",0x5fe0},{"wdf_input",0x5df4},{"wdf_output",0x5df8},
    {"old_dispatch",0x5fb},{"ioctl_done",0x664},{"tap_continue",0x2760},
    {"gap_continue",0x28e7},{"motion_continue",0x2afb},{"queue_packet",0x35ca},
    {"init_continue",0x6469},{"reset_continue",0x2806},{"init_failed",0x658a},
    {"wdf_lock_create",0x5eac},{"wdf_lock_acquire",0x5eb0},{"wdf_lock_release",0x5eb4},
    {"wdf_timer_create",0x5eb8},{"wdf_timer_start",0x5ebc},{"wdf_timer_stop",0x5ec0},
    {"wdf_timer_parent",0x5ec4},{"wdf_context",0x5ce8},{"context_type",0x570c},
    {"queue_continue",0x35cf},{"cleanup_continue",0x628b},
    {"dequeue_packet",0x3564},{"read_continue",0xf2c},{"read_forward",0xfcd},
    {"read_send",0x102f},{"usb_complete",0xe0c},
    {"wdf_format",0x5dac},{"wdf_completion",0x5dd0},{"wdf_target",0x5a68},
    {"wdf_irp",0x5e34},{"wdf_complete",0x5ddc}};
static const Hook hooks[]={
    {0x5f3,8,"runtime_dispatch"},{0x2758,8,"runtime_tap"},{0x28e2,5,"runtime_gap"},
    {0x294f,99,"runtime_motion"},{0x645f,10,"runtime_init"},{0x27f9,13,"runtime_reset"},
    {0x35ca,5,"runtime_queue"},{0x6286,5,"runtime_cleanup"},
    {0x27e6,5,"runtime_click",1},{0x27b0,5,"runtime_click",1},
    {0x2af2,9,"runtime_idle"},{0xf20,12,"runtime_read"}};

static void require(int ok,const char *message)
{
    if(!ok) { fprintf(stderr,"%s\n",message);exit(1); }
}
static void *at(Buffer b,size_t offset,size_t size)
{
    require(offset<=b.size&&size<=b.size-offset,"The object file is incomplete.");
    return b.data+offset;
}
static Buffer read_file(const WCHAR *path)
{
    Buffer b; FILE *f=NULL; long length;
    require(!_wfopen_s(&f,path,L"rb"),"The program cannot open an input file.");
    require(!fseek(f,0,SEEK_END),"The program cannot read the file size.");
    length=ftell(f);require(length>=0&&!fseek(f,0,SEEK_SET),"The file size is incorrect.");
    b.size=(size_t)length;b.data=(BYTE*)malloc(b.size?b.size:1);
    require(b.data!=NULL,"Memory allocation failed.");
    require(fread(b.data,1,b.size,f)==b.size,"The program cannot read the full input file.");
    fclose(f);return b;
}
static WORD word(const BYTE *p) { WORD n;memcpy(&n,p,2);return n; }
static DWORD dword(const BYTE *p) { DWORD n;memcpy(&n,p,4);return n; }
static void put16(BYTE *p,WORD n) { memcpy(p,&n,2); }
static void put32(BYTE *p,DWORD n) { memcpy(p,&n,4); }
static const char *symbol_name(Buffer obj,const IMAGE_SYMBOL *s,size_t strings,char short_name[9])
{
    size_t pos;
    if(s->N.Name.Short) { memcpy(short_name,s->N.ShortName,8);short_name[8]=0;return short_name; }
    require(s->N.Name.Long<=obj.size-strings,"The symbol name offset is incorrect.");
    pos=strings+s->N.Name.Long;
    require(memchr(at(obj,pos,1),0,obj.size-pos)!=NULL,"The symbol name is incomplete.");
    return (char*)obj.data+pos;
}
static int compare(const void *a,const void *b)
{
    DWORD x=*(const DWORD*)a,y=*(const DWORD*)b;return (x>y)-(x<y);
}
static Buffer link_driver(Buffer original,Buffer obj)
{
    IMAGE_FILE_HEADER *header=(IMAGE_FILE_HEADER*)at(obj,0,sizeof(*header));
    IMAGE_SECTION_HEADER *section=NULL;IMAGE_SYMBOL *symbols;
    size_t strings,i,j,count=0,capacity,cursor,start,block,reloc_rva,reloc_size;
    DWORD pe=dword(original.data+60),opt=pe+24,old_rva=dword(original.data+opt+136);
    DWORD old_size=dword(original.data+opt+140),*relocs,entry_points[COUNT(hooks)]={0},old_checksum,new_checksum;
    WORD section_number=0;Buffer image;BYTE *code;
    require(header->Machine==IMAGE_FILE_MACHINE_I386&&!header->SizeOfOptionalHeader,"Use an x86 COFF object.");
    require(header->PointerToSymbolTable<=obj.size&&
        header->NumberOfSymbols<=(obj.size-header->PointerToSymbolTable)/sizeof(*symbols),"The symbol table is incomplete.");
    symbols=(IMAGE_SYMBOL*)at(obj,header->PointerToSymbolTable,header->NumberOfSymbols*sizeof(*symbols));
    strings=header->PointerToSymbolTable+header->NumberOfSymbols*sizeof(*symbols);
    for(i=0;i<header->NumberOfSections;i++) {
        IMAGE_SECTION_HEADER *s=(IMAGE_SECTION_HEADER*)at(obj,sizeof(*header)+i*sizeof(*s),sizeof(*s));
        if(!memcmp(s->Name,".text",5)) { section=s;section_number=(WORD)(i+1);break; }
    }
    require(section!=NULL,"The object file has no code section.");
    code=(BYTE*)at(obj,section->PointerToRawData,section->SizeOfRawData);
    capacity=old_size/2+section->NumberOfRelocations;
    relocs=(DWORD*)malloc(capacity*sizeof(*relocs));
    image.size=original.size+section->SizeOfRawData;
    image.data=(BYTE*)calloc(image.size+3+capacity*12+127,1);
    require(relocs&&image.data,"Memory allocation failed.");
    memcpy(image.data,original.data,original.size);
    memcpy(image.data+original.size,code,section->SizeOfRawData);
    for(i=0;i<header->NumberOfSymbols;i+=1+symbols[i].NumberOfAuxSymbols) {
        char name[9];const char *text=symbol_name(obj,&symbols[i],strings,name);
        if(symbols[i].SectionNumber!=section_number)continue;
        for(j=0;j<COUNT(hooks);j++)if(!strcmp(text,hooks[j].name))entry_points[j]=(DWORD)original.size+symbols[i].Value;
    }
    for(i=0;i<section->NumberOfRelocations;i++) {
        IMAGE_RELOCATION *r=(IMAGE_RELOCATION*)at(obj,section->PointerToRelocations+i*sizeof(*r),sizeof(*r));
        IMAGE_SYMBOL *s;DWORD target=0,addend;BYTE *site;char name[9];const char *text;
        require(r->SymbolTableIndex<header->NumberOfSymbols&&section->SizeOfRawData>=4&&
            r->VirtualAddress<=section->SizeOfRawData-4,"The code relocation is outside the object data.");
        s=&symbols[r->SymbolTableIndex];text=symbol_name(obj,s,strings,name);
        if(s->SectionNumber==section_number)target=(DWORD)original.size+s->Value;
        else for(j=0;j<sizeof(externs)/sizeof(externs[0]);j++)if(!strcmp(text,externs[j].name))target=externs[j].address;
        require(target!=0,"The object file refers to an unknown symbol.");
        site=image.data+original.size+r->VirtualAddress;addend=dword(site);
        if(r->Type==IMAGE_REL_I386_REL32)put32(site,target+addend-(DWORD)original.size-r->VirtualAddress-4);
        else {
            require(r->Type==IMAGE_REL_I386_DIR32,"The COFF relocation type is not supported.");
            put32(site,0x10000+target+addend);relocs[count++]=(DWORD)original.size+r->VirtualAddress;
        }
    }
    for(i=0;i<COUNT(hooks);i++) {
        require(entry_points[i]!=0,"A runtime entry point is missing.");
        image.data[hooks[i].offset]=hooks[i].call?0xe8:0xe9;
        put32(image.data+hooks[i].offset+1,entry_points[i]-hooks[i].offset-5);
        memset(image.data+hooks[i].offset+5,0x90,hooks[i].length-5);
    }
    memset(image.data+0x2753,0x90,5);memset(image.data+0x28dd,0x90,5);
    image.data[0x28e7]=0x73;put32(image.data+0x5708,0x1910);put32(image.data+0x6448,0x1910);
    for(cursor=old_rva;cursor<old_rva+old_size;cursor+=block) {
        DWORD page=dword(original.data+cursor);block=dword(original.data+cursor+4);
        for(i=cursor+8;i<cursor+block;i+=2) {
            WORD entry=word(original.data+i);DWORD target=page+(entry&4095);
            if(!(entry>>12))continue;
            if(target==0x2802||target==0x2955)put16(image.data+i,entry&4095);
            else {
                for(j=0;j<COUNT(hooks);j++)require(target<hooks[j].offset||target>=hooks[j].offset+hooks[j].length,
                    "A hook overlaps a base relocation.");
                relocs[count++]=target;
            }
        }
    }
    qsort(relocs,count,sizeof(*relocs),compare);
    image.size=(image.size+3)&~(size_t)3;reloc_rva=image.size;
    for(i=0;i<count;) {
        DWORD page=relocs[i]&~4095u;start=image.size;image.size+=8;
        do {
            put16(image.data+image.size,(WORD)(0x3000|(relocs[i]&4095)));image.size+=2;i++;
            while(i<count&&relocs[i]==relocs[i-1])i++;
        } while(i<count&&(relocs[i]&~4095u)==page);
        image.size=(image.size+3)&~(size_t)3;
        put32(image.data+start,page);put32(image.data+start+4,(DWORD)(image.size-start));
    }
    reloc_size=image.size-reloc_rva;image.size=(image.size+127)&~(size_t)127;
    start=opt+224+(word(original.data+pe+6)-1)*40;
    put32(image.data+start+8,(DWORD)image.size-dword(original.data+start+12));
    put32(image.data+start+16,dword(image.data+start+8));put32(image.data+start+36,0x68000040);
    put32(image.data+opt+56,(DWORD)image.size);
    put32(image.data+opt+8,dword(original.data+opt+8)+(DWORD)(image.size-original.size));
    put32(image.data+opt+136,(DWORD)reloc_rva);put32(image.data+opt+140,(DWORD)reloc_size);
    require(CheckSumMappedFile(image.data,(DWORD)image.size,&old_checksum,&new_checksum)!=NULL,
        "The program cannot calculate the driver checksum.");
    put32(image.data+opt+64,new_checksum);free(relocs);return image;
}
static void write_header(FILE *f,Buffer original,Buffer image)
{
    size_t i=0,start,j;
    fprintf(f,"#define SOURCE_SIZE %zuu\n#define TARGET_SIZE %zuu\n",original.size,image.size);
    fputs("static const unsigned char source_hash[32] = {",f);
    for(j=0;j<32;j++)fprintf(f,"%s0x%02x",j?",":"",source_hash[j]);
    fputs("};\nstatic void patch(unsigned char *data)\n{\n",f);
    while(i<image.size) {
        if(i<original.size&&original.data[i]==image.data[i]) { i++;continue; }
        start=i;
        while(i<image.size&&(i>=original.size||original.data[i]!=image.data[i]))i++;
        fprintf(f,"    memcpy(data + %zu, \"",start);
        for(j=start;j<i;j++)fprintf(f,"\\x%02x",image.data[j]);
        fprintf(f,"\", %zu);\n",i-start);
    }
    fputs("}\n",f);
}
int wmain(int argc,WCHAR **argv)
{
    Buffer original,obj,image;BYTE digest[32];FILE *f=NULL;int ok;
    require(argc==5,"Command: build-patch ORIGINAL.sys RUNTIME.obj OUTPUT.sys PATCH.h");
    original=read_file(argv[1]);
    require(hash_sha256(original.data,(DWORD)original.size,digest),"The program cannot calculate the driver hash.");
    require(!memcmp(digest,source_hash,32),"Use AppleMTP.sys 3.1.0.10 from Boot Camp 3.2 x86.");
    obj=read_file(argv[2]);image=link_driver(original,obj);
    require(!_wfopen_s(&f,argv[3],L"wbx"),"The program cannot make the driver output. Use a new file name.");
    ok=fwrite(image.data,1,image.size,f)==image.size;if(fclose(f))ok=0;
    require(ok,"The program cannot write the full driver output.");
    require(!_wfopen_s(&f,argv[4],L"wbx"),"The program cannot make the patch header. Use a new file name.");
    write_header(f,original,image);ok=!ferror(f);if(fclose(f))ok=0;
    require(ok,"The program cannot write the full patch header.");
    free(image.data);free(obj.data);free(original.data);
    puts("The driver and patch code are ready.");return 0;
}
