#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>

#include "xml.h"


/*
----------------
simple XML-Parser that reads Attributes from XML-tags.
Actual Content of XML-Elements is discarded, as LMMS doesn't
use theese outside the notation.
----------------
*/


/*Wrappers for allocation*/
static void _xml_free(void *ptr)
{
 free(ptr);
}

static void *_xml_alloc(size_t len)
{
 return malloc(len);
}

static void *_xml_calloc(size_t len)
{
 return calloc(len,1);
}

static void *_xml_realloc(void *ptr,size_t len)
{
 return realloc(ptr,len);
}

/*frees string inside XML_TOKEN structure*/
int _xml_clean_token(struct XML_TOKEN *t)
{
 int a;
 if(t==NULL) return -1;
 for(a=t->var_count;a<t->_var_space;a++)
 { 
  if(t->variable[a]!=NULL) _xml_free(t->variable[a]);
  if(t->content[a]!=NULL) _xml_free(t->content[a]);
  t->variable[a]=NULL;
  t->content[a]=NULL;  
 }
 return 0;
}

/* frees XML-tokens*/
int xml_free_token(struct XML_TOKEN *t)
{
 if(t==NULL) return -1;
 _xml_clean_token(t);
 if(t->variable!=NULL) _xml_free(t->variable);
 if(t->content!=NULL) _xml_free(t->content);
 t->variable=NULL;
 t->content=NULL;  
 _xml_free(t);
 return 0;
}

/*set number of attributs in XML-TOKEN  (attr0 is element name)*/
int _xml_token_setsize(struct XML_TOKEN *t, int newsize)
{
 int tmp;
 if(t==NULL) return -1;
 if(t->_var_space==newsize) return 0;
 if(newsize<t->_var_space)
 {
  tmp=t->var_count;
  if(tmp>newsize) tmp=newsize;
  t->var_count=newsize;  
  _xml_clean_token(t); 
  t->var_count=tmp;
  t->variable=_xml_realloc(t->variable,(newsize+1)*sizeof(char *));
  t->content=_xml_realloc(t->content,(newsize+1)*sizeof(char *));
  return 0;
 }
 else  /*newsize>t->_var_space*/
 {
  t->variable=_xml_realloc(t->variable,(newsize+1)*sizeof(char *));
  t->content=_xml_realloc(t->content,(newsize+1)*sizeof(char *));
  while(newsize>t->_var_space)
  {
   tmp=t->_var_space++;
   t->variable[tmp]=NULL;
   t->content[tmp]=NULL;                    
  }
  return 0;     
 }
 return 0;
}

/*alocate XML-Token structure*/
struct XML_TOKEN *xml_alloc_token(void)
{
 struct XML_TOKEN *t=NULL;
 t=_xml_calloc(sizeof (struct XML_TOKEN));
 if(t==NULL) return NULL;
 _xml_token_setsize(t,8);
 return t;
}

/*free XML-Kontext*/
int xml_free_context(struct XML_CONTEXT *k)
{
 if(k==NULL) return -1;
 if(k->_buffer!=NULL) _xml_free(k->_buffer);
 k->_buffer=NULL;
 _xml_free(k);
}

/*alloc XML-Kontext*/
struct XML_CONTEXT *xml_alloc_context(void)
{
 int tmp;
 struct XML_CONTEXT *k=NULL;
 k=_xml_calloc(sizeof (struct XML_CONTEXT));
 if(k==NULL) return NULL;
 tmp=256;
 k->_buffer=_xml_calloc(tmp*2);
 if(k->_buffer!=NULL) 
  k->_buff_size=tmp;
 return k;
}

/*default handler for _xgetchar to point to for stdio FILE as input*/
int _xml_getchar_0(void *k)
{
 return getc(k);    
}

/*default handler for _xungetchar to point to for stdio FILE as input*/
int _xml_ungetchar_0(void *k, int ch)
{
 return ungetc(ch,k);     
}

/*sets a given stdio FILE as input*/
int xml_set_context_file(struct XML_CONTEXT *k, FILE *file)
{
 if(k==NULL) return -1;
 k-> _xgetchar=_xml_getchar_0;
 k-> _xungetchar=_xml_ungetchar_0;
 k->_inner_context=file;
 return 0; 
}

/*calls getchar()-handler from kontext*/
int xml_getchar(struct XML_CONTEXT *k)
{
 if(k==NULL) return EOF;
 return k->_xgetchar(k->_inner_context); 
}

/*calls ungetchar()-handler from kontext*/
int xml_ungetchar(struct XML_CONTEXT *k, int ch)
{
 if(k==NULL) return -1;
 return k->_xungetchar(k->_inner_context,ch); 
}


/*sets pointet to char* to a string with copied text as data*/
static void _setnewstring(char **var,const char *text)
{
 if(*var==NULL && text==NULL) return;
 if(*var==NULL) *var=_xml_alloc(strlen(text)+1);
 else if(text==NULL)
 {
  _xml_free(*var);
  *var=NULL;
  return;
 }
 else
 {
  *var=_xml_realloc(*var,strlen(text)+1);
 } 
 strcpy(*var,text); 
}


/*add attribute to XML-Token*/
int xml_addvariable(struct XML_TOKEN *t,const char *vname,const char *vcontent)
{
 int tmp;
 if(t==NULL) return -1;
 tmp=t->var_count;
 if(tmp>=t->_var_space)
  _xml_token_setsize(t,tmp+1);  
 _setnewstring(&(t->variable[tmp]),vname);
 _setnewstring(&(t->content[tmp]),vcontent);
 t->var_count++;
 return 0;
}

/*clears strings in xml-token without freeing buffers*/
int _xml_reset_token(struct XML_TOKEN *t)
{
 if(t==NULL) return -1;
 t->var_count=0;
 t->close_token=t->alone_token=0;   
 if(t->variable!=NULL)
  if(t->variable[0]!=NULL)
   t->variable[0][0]=0;
 if(t->content!=NULL)
  if(t->content[0]!=NULL)
   t->content[0][0]=0;
 return 0;
}

/*clear and free strings in a token */
int xml_reset_token(struct XML_TOKEN *t)
{
 if(t==NULL) return -1;
 _xml_reset_token(t);
 _xml_clean_token(t);
 return 0;
}



#define CHAR_BLANK   1
#define CHAR_BRLEFT  2
#define CHAR_BRRIGHT 3
#define CHAR_SLASH   4
#define CHAR_QUOTE   5
#define CHAR_OTHER   6
#define CHAR_EOF     7
#define CHAR_EQUAL   8
#define CHAR_SQBR    9

/* simple lexer returns symbol for special characters*/
static int _chartype(int ch)
{
 if(ch==EOF/*||ch==0*/) return CHAR_EOF;
 switch(ch&0xFF)
 {
  case ' ':
  case 9: /*Tab*/
  case 13: /*CR*/
  case 10: /*LF*/
  case 8: /*Backspace*/
  case 127: /*DEL*/
  case 255: /*DEL2*/
  case 0: /*NUL*/
   return CHAR_BLANK;
  case '<': 
   return CHAR_BRLEFT;
  case '>':
   return CHAR_BRRIGHT;
  case '/':
   return CHAR_SLASH;
  case 34: /* " */
   return CHAR_QUOTE;  
  case '=':
   return CHAR_EQUAL;  
  case '[':
  case ']':
   return CHAR_SQBR;
 }
 return CHAR_OTHER;     
}


/*reads next xml-token. Places name in attribute 0 and atrributes in following attributes*/
int xml_read_token(struct XML_TOKEN *t,struct XML_CONTEXT *k,int skiptext)
{
 int ch,typ,loop,len,inquote,incontent=0,cdata=0,empty=0;
 
 if(t==NULL||k==NULL) return -1;

 _xml_reset_token(t);

 k->_buffer[0]=0;
 k->_buffer[k->_buff_size]=0;

 /*Skip Blankspace*/
 for(loop=1;loop;)
 { 
  ch=xml_getchar(k);
  switch(_chartype(ch))
  {
   case CHAR_BLANK:
    continue;
   case CHAR_BRLEFT:
    loop=0;
    break;
   case CHAR_BRRIGHT:
   case CHAR_SLASH:
   case CHAR_QUOTE:
   case CHAR_OTHER:
   case CHAR_EQUAL:
   case CHAR_SQBR:
    if(skiptext) continue;
    xml_ungetchar(k,ch);
    return 0;
   case CHAR_EOF:  
    return 1;                 
  }
 }  

 incontent=0;
READ_NAME:
 /*Read Name*/
 len=0;
 inquote=0;
 empty=0;
 for(loop=1;loop;)
 { 
  ch=xml_getchar(k);
  switch(_chartype(ch))
  {
   case CHAR_BLANK:
    if(inquote) goto TOKNAME_LITERAL;
    if(len||empty) loop=0;
    break;
   case CHAR_BRLEFT:
   /* if(inquote)*/ goto TOKNAME_LITERAL;
/*    if(!skiptext)
     return -2;*/ /*Syntax Error*/
    break;
   case CHAR_EQUAL:
    if(inquote) goto TOKNAME_LITERAL;
    k->_buffer[incontent+len]=0;
    incontent=k->_buff_size;
    goto READ_NAME;
    break;
   case CHAR_SQBR:
     if(ch=='[')
     {
      if(len>=8&&!cdata)
      {
      if(!memcmp( &(k->_buffer[incontent+len-8]),"<![CDATA",8))
       {
        len-=8;
        inquote=1;
        cdata=1;
        continue;
       }
      }
      if(len==7&&!cdata)
      {
       if(!memcmp( &(k->_buffer[incontent+len-7]),"![CDATA",7))
       {
        len-=7;
        inquote=1;
        cdata=2;
         continue;
       }
      }
     }
     else
     {
      if(len+4 >k->_buff_size)
       len-=4;
     }
     goto TOKNAME_LITERAL;
   case CHAR_BRRIGHT:
     if(len>=2&&cdata)
     {
      if(!memcmp( &(k->_buffer[incontent+len-2]),"]]",2))
      {
       len-=2;
       inquote=0;
       if(cdata==2)
       {
        t->alone_token=1;       
        /*printf(" [CDATA-end]");*/
        cdata=0;
        loop=0;
        break;
       }
       cdata=0;
/*       printf("CDATA ends");*/
       continue;
      }
     }        
    if(inquote) goto TOKNAME_LITERAL;
/*    xml_ungetchar(k,ch); */
    loop=0;
    if(!len) return 0;
    break;
   case CHAR_SLASH:
    if(inquote) goto TOKNAME_LITERAL;
    if(!len)
    {
     if(!t->var_count)
      t->close_token=1;
     else
      t->alone_token=1;
    }
    else    
    {
     xml_ungetchar(k,ch); 
     loop=0;
    }
    break;
   case CHAR_QUOTE:
    if(cdata) goto TOKNAME_LITERAL;
    inquote=!inquote;
    if(!inquote) empty=1;
    break;
   case CHAR_OTHER:
    if( (ch=='!'||ch=='?') && !inquote && !len && !cdata) t->alone_token=1;  /*internal one*/
    
   
TOKNAME_LITERAL:        
    if(len+1 <k->_buff_size)
     k->_buffer[incontent+(len++)]=ch;
    break;
   case CHAR_EOF:   
    loop=0;
    break;                
  }
 }  
 k->_buffer[incontent+len]=0;

 if(incontent)
  xml_addvariable(t,k->_buffer,&(k->_buffer[k->_buff_size]));
 else
  xml_addvariable(t,k->_buffer,NULL);

 incontent=0;

 switch(_chartype(ch))
 {
  case CHAR_BLANK:
  case CHAR_SLASH:
  case CHAR_OTHER:
  case CHAR_SQBR:
   goto READ_NAME;
  case CHAR_BRRIGHT:
   return 0;
  case CHAR_EOF: 
/*    printf("EOF!");*/
   return 1;                  
 }

 return 0;
}

/*read tokens until close token with name encountered.
 if name is NULL any close token will do.
 if new blocks are encountered they are recursivly skipped*/
int xml_skiptoclose(struct XML_CONTEXT *context,const char *name)
{
 int r=0;
 struct XML_TOKEN *token=NULL;
 
/* printf("skip:%s ",name);*/
 
 token=xml_alloc_token();
 if(token!=NULL)
 {   
  do{
   r=xml_read_token(token,context,1);
   if(token->close_token)
   {
    /*printf("{Close-token}");*/
    if(name==NULL) break;
    if(!strcmp(name,token->variable[0])) break;                      
    /*printf("***");*/
   }
   else if( (!(token->alone_token||token->close_token))&&(token->variable[0]!=NULL))
   {
    /*printf(" from:%s-",name);*/
    xml_skiptoclose(context,token->variable[0]);
   }
  }while(!r);
  
  xml_free_token(token);    
 }      
/* if(r)
  printf("error(%s)\n",name);
 else
  printf("OK(%s)\n",name);*/
 return r;
}

/*returns 1-based index if string is found, 0 otherwise*/
int xml_lookupname(struct XML_TOKEN *t,const char *stringtable,int n)
/*  "String1\0String2\0String3\0\0"*/
{
 char *tptr1,*tptr2;
 const char *sptr;
 int r=1,flag;
 
 if(stringtable==NULL) return 0;
 if(t->variable==NULL) return 0;
 if(t->var_count<=n) return 0;
 if(t->variable[n]==NULL) return 0;    

 tptr1=t->variable[n];
 sptr=stringtable;
 
 while(*sptr)
 {
  flag=1;
  tptr2=tptr1;
  while((*tptr2)==(*sptr))
  {
   if( !((*tptr2) | (*sptr)) ) return r;   
   tptr2++;
   sptr++;
  }
  r++;
  while(*sptr) sptr++;
  sptr++; 
 }
 return 0;
}

/*returns integer in attribute number n*/
int xml_readvar_int(struct XML_TOKEN *t,int n)
{
 const char *sptr;
 int r=0,ctr=0,minus=0;
 if(t==NULL) return 0;
 if(t->var_count<=n) return 0;
 if(t->content[n]==NULL) return 0;
 sptr=t->content[n];
 while(*sptr)
 {
  if(*sptr=='-')
  {
   if(r) return r;
   else minus=1;
  }
  else if((*sptr)>='0' && (*sptr)<='9')
  {  
   r= ((int)((*sptr)-'0')) + r*10;
  }
  else
  {
   if(*sptr=='.'||r) break;    
  }
  sptr++;
  ctr++;
 }
 if(minus) return -r;
 return r;
}

/*returns float in attribute number n*/
float xml_readvar_float(struct XML_TOKEN *t,int n)
{
 const char *sptr;
 int r=0,ctr=0,minus=0,divide=1,dot=0;
 if(t==NULL) return 0;
 if(t->var_count<=n) return 0;
 if(t->content[n]==NULL) return 0;
 sptr=t->content[n];
 while(*sptr)
 {
  if(*sptr=='-')
  {
   if(r) return r;
   else minus=1;
  }
  else if((*sptr)>='0' && (*sptr)<='9')
  {  
   r= ((int)((*sptr)-'0')) + r*10;
   if(dot) divide*=10;
  }
  else if(*sptr=='.')
  {
   if(dot) return 0;
   else dot=1;
  }
  sptr++;
  ctr++;
 }
 
 if(minus) r=-r;
 return ((float) r )/((float) divide);
}
