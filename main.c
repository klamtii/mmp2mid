#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "xml.h"
#include "midi.h"

/*
-----------
main()-function and command-line parser.
----------
*/



int parser_verbose;  /*parser.c*/
int convert_mmp2mid(const char *destname,const char *srcname); /*convert.c*/
int multitrack; /*convert.c*/

/*used by the help*/
unsigned version_major=1,version_minor=0;

/*  "String1\0String2\0String3\0\0" returns 1-based index if found, 0 otherwise*/
static int lookupstr(const char *str, const char *table)
{
 const char *str2;
 int r=1;
 if(str==NULL||table==NULL) return -1;
 
 while(*table)
 {
  str2=str;
  while((*str2)==(*table))
  {
   if( !((*str2) | (*table)) ) return r;   
   str2++;
   table++;
  }
  r++;
  while(*table) table++;
  table++; 
 }
 return 0;       
}


/*Prints inline-help to stdout*/
static int print_help(const char *progname)
{
 printf("Converts LMMS-Project <filename>.mmp to MIDI\n\n");
 printf("Version %d.%d\n",version_major,version_minor);
 printf("%s [-v[+]] [-1track] [-norunstat] <filename>.mmp [...]" 
             "[-o <outfile>.mid [...]]\n\n",progname);      
 printf("-v          Be Verbose; -v+ Be more Verbose\n");
 printf("-1track     Make singletrack (type 0) MIDI-File instead multitrack (type 1)\n");
 printf("-norunstate Do not use Running Status (omited command)\n");
 printf("<filename>.mmp The LMMS-Project to convert (must be uncompressed)\n");
 printf("<outfile>.mid  MIDI-File to write instead of <filename>.mid\n");
 
 return 0;      
}

/* main() function */

int main(int argc,char **argv)
{
 int a,b;
 int max_infile=100; /*index in argv  there outfiles follow*/
 int buflen=100;
 
 int infile_index,outfile_index;
 
 char *infile_name=NULL;
 char *outfile_name=NULL;
 
 if(argc<2)
 {
  printf("Missing argument. type %s -? for help\n",argv[0]);
  return -1;
 }
 
 max_infile=argc;
 
 for(a=0;a<argc;a++)
 {
  b=strlen(argv[a]);
  if(b>buflen) buflen=b;
  switch(lookupstr(argv[a],"-?\0/?\0-h\0-v\0-v+\0-1track\0-norunstate\0-o\0-O\0\0"))
  {
   case 1: case 2: case 3:
    return print_help(argv[0]);
   case 4:
    parser_verbose=1;
    break;
   case 5:
    parser_verbose=2;
    break;
   case 6:
    multitrack=0;
    break;
   case 7:
    midi_running_status=0;
    break;
   case 8: case 9:
    max_infile=a;
    break;
   default:
    if(argv[a][0]=='-')
    {
     printf("Unknown Command-Line Switch: %s\n",argv[a]);
     return -1;
    }
    break;
  }  
 }
 
 infile_name=malloc(buflen+10);
 outfile_name=malloc(buflen+10);
 
 if(outfile_name==NULL) 
 {
  printf("Cannot alloc() buffer.\n");
  return -1;
 }

 outfile_index=max_infile;

 for(infile_index=1;infile_index<max_infile;infile_index++)
 {
  if(argv[infile_index][0]=='-') continue;
  while(outfile_index<argc)
  {
   if(argv[outfile_index][0]!='-') break;
   outfile_index++;
  }
  
  strcpy(infile_name,argv[infile_index]);
  b=strlen(infile_name);
  a=b-4;
  if(a<0) a=0;
  while(infile_name[a]) if(infile_name[a++]=='.') break;
  if(!infile_name[a]) /*non extension ?*/
  {
   FILE *file;
   file=fopen(infile_name,"rt");
   if(file!=NULL) fclose(file);
   else strcat(infile_name,".mmp");
  }
  
  if(outfile_index<argc)
   strcpy(outfile_name,argv[outfile_index]);
  else
  {
   strcpy(outfile_name,infile_name);
   b=strlen(outfile_name)-1;
   for(a=0;a<4;a++)
    if(b&&outfile_name[b]!='.'&&outfile_name[b]!='\\'&&outfile_name[b]!='/') b--;
   if(outfile_name[b]!='.') b=strlen(outfile_name);
   strcpy(outfile_name+b,".mid");
  }
  
  if(parser_verbose)
   printf("Converting %s to %s\n",infile_name,outfile_name);
  convert_mmp2mid(outfile_name,infile_name);
 }
 
 
 if(outfile_name!=NULL)
  free(outfile_name);
 outfile_name=NULL;
 
 /*system("pause");*/
 
 return 0;   
}
