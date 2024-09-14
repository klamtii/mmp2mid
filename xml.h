#include <stddef.h>
#include <stdint.h>


struct XML_TOKEN{
 char **variable;
 char **content;
 int var_count;
 int _var_space;
 int close_token;
 int alone_token;
 int syntax_error;
};

struct XML_CONTEXT{
 void *_inner_context;
 int (*_xgetchar)(void *k);
 int (*_xungetchar)(void *k, int ch);
 char *_buffer;
 int _buff_size;
 int eof;
};


int _xml_clean_token(struct XML_TOKEN *t);
int xml_free_token(struct XML_TOKEN *t);
int _xml_token_setsize(struct XML_TOKEN *t, int newsize);
struct XML_TOKEN *xml_alloc_token(void);
int xml_free_context(struct XML_CONTEXT *k);
struct XML_CONTEXT *xml_alloc_context(void);
int _xml_getchar_0(void *k);
int _xml_ungetchar_0(void *k, int ch);
int xml_set_context_file(struct XML_CONTEXT *k, FILE *file);
int xml_getchar(struct XML_CONTEXT *k);
int xml_ungetchar(struct XML_CONTEXT *k, int ch);
int xml_addvariable(struct XML_TOKEN *t,const char *vname,const char *vcontent);
int _xml_reset_token(struct XML_TOKEN *t);
int xml_reset_token(struct XML_TOKEN *t);
int xml_read_token(struct XML_TOKEN *t,struct XML_CONTEXT *k,int skiptext);
int xml_lookupname(struct XML_TOKEN *t,const char *stringtable,int n);
int xml_readvar_int(struct XML_TOKEN *t,int n);
float xml_readvar_float(struct XML_TOKEN *t,int n);
