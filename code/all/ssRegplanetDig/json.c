/*
  Copyright (c) 2009 Dave Gamble

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

// json
// JSON parser in C.

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include "json.h"

static int json_strcasecmp(const char *s1,const char *s2)
{
	if (!s1) return (s1==s2)?0:1;if (!s2) return 1;
	for(; tolower(*s1) == tolower(*s2); ++s1, ++s2)	if(*s1 == 0)	return 0;
	return tolower(*(const unsigned char *)s1) - tolower(*(const unsigned char *)s2);
}

static void *(*json_malloc)(size_t sz) = malloc;
static void (*json_free)(void *ptr) = free;

static char* json_strdup(const char* str)
{
      size_t len;
      char* copy;

      len = strlen(str) + 1;
      if (!(copy = (char*)json_malloc(len))) return 0;
      memcpy(copy,str,len);
      return copy;
}

void json_InitHooks(json_Hooks* hooks)
{
    if (!hooks) { /* Reset hooks */
        json_malloc = malloc;
        json_free = free;
        return;
    }

	json_malloc = (hooks->malloc_fn)?hooks->malloc_fn:malloc;
	json_free	 = (hooks->free_fn)?hooks->free_fn:free;
}

// Internal constructor.
static json *json_New_Item()
{
	json* node = (json*)json_malloc(sizeof(json));
	if (node) memset(node,0,sizeof(json));
	return node;
}

// Delete a json structure.
void json_Delete(json *c)
{
	json *next;
	while (c)
	{
		next=c->next;
		if (!(c->type&json_IsReference) && c->child) json_Delete(c->child);
		if (!(c->type&json_IsReference) && c->valuestring) json_free(c->valuestring);
		if (c->string) json_free(c->string);
		json_free(c);
		c=next;
	}
}

// Parse the input text to generate a number, and populate the result into item.
static const char *parse_number(json *item,const char *num)
{
	double n=0,sign=1,scale=0;int subscale=0,signsubscale=1;

	// Could use sscanf for this?
	if (*num=='-') sign=-1,num++;	// Has sign?
	if (*num=='0') num++;			// is zero
	if (*num>='1' && *num<='9')	do	n=(n*10.0)+(*num++ -'0');	while (*num>='0' && *num<='9');	// Number?
	if (*num=='.') {num++;		do	n=(n*10.0)+(*num++ -'0'),scale--; while (*num>='0' && *num<='9');}	// Fractional part?
	if (*num=='e' || *num=='E')		// Exponent?
	{	num++;if (*num=='+') num++;	else if (*num=='-') signsubscale=-1,num++;		// With sign?
		while (*num>='0' && *num<='9') subscale=(subscale*10)+(*num++ - '0');	// Number?
	}

	n=sign*n*pow(10.0,(scale+subscale*signsubscale));	// number = +/- number.fraction * 10^+/- exponent
	
	item->valuedouble=n;
	item->valueint=(int)n;
	item->type=json_Number;
	return num;
}

// Render the number nicely from the given item into a string.
static char *print_number(json *item)
{
	char *str;
	double d=item->valuedouble;
	if (fabs(((double)item->valueint)-d)<=DBL_EPSILON && d<=INT_MAX && d>=INT_MIN)
	{
		str=(char*)json_malloc(21);	// 2^64+1 can be represented in 21 chars.
		sprintf(str,"%d",item->valueint);
	}
	else
	{
		str=(char*)json_malloc(64);	// This is a nice tradeoff.
		if (fabs(floor(d)-d)<=DBL_EPSILON)			sprintf(str,"%.0f",d);
		else if (fabs(d)<1.0e-6 || fabs(d)>1.0e9)	sprintf(str,"%e",d);
		else										sprintf(str,"%f",d);
	}
	return str;
}

// Parse the input text into an unescaped cstring, and populate item.
static const char firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
static const char *parse_string(json *item,const char *str)
{
	const char *ptr=str+1;char *ptr2;char *out;int len=0;unsigned uc;
	if (*str!='\"') return 0;	// not a string!
	
	while (*ptr!='\"' && (unsigned char)*ptr>31 && ++len) if (*ptr++ == '\\') ptr++;	// Skip escaped quotes.
	
	out=(char*)json_malloc(len+1);	// This is how long we need for the string, roughly.
	if (!out) return 0;
	
	ptr=str+1;ptr2=out;
	while (*ptr!='\"' && (unsigned char)*ptr>31)
	{
		if (*ptr!='\\') *ptr2++=*ptr++;
		else
		{
			ptr++;
			switch (*ptr)
			{
				case 'b': *ptr2++='\b';	break;
				case 'f': *ptr2++='\f';	break;
				case 'n': *ptr2++='\n';	break;
				case 'r': *ptr2++='\r';	break;
				case 't': *ptr2++='\t';	break;
				case 'u':	 // transcode utf16 to utf8. DOES NOT SUPPORT SURROGATE PAIRS CORRECTLY.
					sscanf(ptr+1,"%4x",&uc);	// get the unicode char.
					len=3;if (uc<0x80) len=1;else if (uc<0x800) len=2;ptr2+=len;
					
					switch (len) {
						case 3: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
						case 2: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
						case 1: *--ptr2 =(uc | firstByteMark[len]);
					}
					ptr2+=len;ptr+=4;
					break;
				default:  *ptr2++=*ptr; break;
			}
			ptr++;
		}
	}
	*ptr2=0;
	if (*ptr=='\"') ptr++;
	item->valuestring=out;
	item->type=json_String;
	return ptr;
}

// Render the cstring provided to an escaped version that can be printed.
static char *print_string_ptr(const char *str)
{
	const char *ptr;char *ptr2,*out;int len=0;
	
	if (!str) return json_strdup("");
	ptr=str;while (*ptr && ++len) {if ((unsigned char)*ptr<32 || *ptr=='\"' || *ptr=='\\') len++;ptr++;}
	
	out=(char*)json_malloc(len+3);
	ptr2=out;ptr=str;
	*ptr2++='\"';
	while (*ptr)
	{
		if ((unsigned char)*ptr>31 && *ptr!='\"' && *ptr!='\\') *ptr2++=*ptr++;
		else
		{
			*ptr2++='\\';
			switch (*ptr++)
			{
				case '\\':	*ptr2++='\\';	break;
				case '\"':	*ptr2++='\"';	break;
				case '\b':	*ptr2++='b';	break;
				case '\f':	*ptr2++='f';	break;
				case '\n':	*ptr2++='n';	break;
				case '\r':	*ptr2++='r';	break;
				case '\t':	*ptr2++='t';	break;
				default: ptr2--;	break;	// eviscerate with prejudice.
			}
		}
	}
	*ptr2++='\"';*ptr2++=0;
	return out;
}
// Invote print_string_ptr (which is useful) on an item.
static char *print_string(json *item)	{return print_string_ptr(item->valuestring);}

// Predeclare these prototypes.
static const char *parse_value(json *item,const char *value);
static char *print_value(json *item,int depth,int fmt);
static const char *parse_array(json *item,const char *value);
static char *print_array(json *item,int depth,int fmt);
static const char *parse_object(json *item,const char *value);
static char *print_object(json *item,int depth,int fmt);

// Utility to jump whitespace and cr/lf
static const char *skip(const char *in) {while (in && (unsigned char)*in<=32) in++; return in;}

// Parse an object - create a new root, and populate.
json *json_Parse(const char *value)
{
	json *c=json_New_Item();
	if (!c) return 0;       /* memory fail */

	if (!parse_value(c,skip(value))) {json_Delete(c);return 0;}
	return c;
}

// Render a json item/entity/structure to text.
char *json_Print(json *item)				{return print_value(item,0,1);}
char *json_PrintUnformatted(json *item)	{return print_value(item,0,0);}

// Parser core - when encountering text, process appropriately.
static const char *parse_value(json *item,const char *value)
{
	if (!value)						return 0;	// Fail on null.
	if (!strncmp(value,"null",4))	{ item->type=json_NULL;  return value+4; }
	if (!strncmp(value,"false",5))	{ item->type=json_False; return value+5; }
	if (!strncmp(value,"true",4))	{ item->type=json_True; item->valueint=1;	return value+4; }
	if (*value=='\"')				{ return parse_string(item,value); }
	if (*value=='-' || (*value>='0' && *value<='9'))	{ return parse_number(item,value); }
	if (*value=='[')				{ return parse_array(item,value); }
	if (*value=='{')				{ return parse_object(item,value); }

	return 0;	// failure.
}

// Render a value to text.
static char *print_value(json *item,int depth,int fmt)
{
	char *out=0;
	if (!item) return 0;
	switch ((item->type)&255)
	{
		case json_NULL:	out=json_strdup("null");	break;
		case json_False:	out=json_strdup("false");break;
		case json_True:	out=json_strdup("true"); break;
		case json_Number:	out=print_number(item);break;
		case json_String:	out=print_string(item);break;
		case json_Array:	out=print_array(item,depth,fmt);break;
		case json_Object:	out=print_object(item,depth,fmt);break;
	}
	return out;
}

// Build an array from input text.
static const char *parse_array(json *item,const char *value)
{
	json *child;
	if (*value!='[')	return 0;	// not an array!

	item->type=json_Array;
	value=skip(value+1);
	if (*value==']') return value+1;	// empty array.

	item->child=child=json_New_Item();
	if (!item->child) return 0;		 // memory fail
	value=skip(parse_value(child,skip(value)));	// skip any spacing, get the value.
	if (!value) return 0;

	while (*value==',')
	{
		json *new_item;
		if (!(new_item=json_New_Item())) return 0; 	// memory fail
		child->next=new_item;new_item->prev=child;child=new_item;
		value=skip(parse_value(child,skip(value+1)));
		if (!value) return 0;	// memory fail
	}

	if (*value==']') return value+1;	// end of array
	return 0;	// malformed.
}

// Render an array to text
static char *print_array(json *item,int depth,int fmt)
{
	char **entries;
	char *out=0,*ptr,*ret;int len=5;
	json *child=item->child;
	int numentries=0,i=0,fail=0;
	
	// How many entries in the array?
	while (child) numentries++,child=child->next;
	// Allocate an array to hold the values for each
	entries=(char**)json_malloc(numentries*sizeof(char*));
	if (!entries) return 0;
	memset(entries,0,numentries*sizeof(char*));
	// Retrieve all the results:
	child=item->child;
	while (child && !fail)
	{
		ret=print_value(child,depth+1,fmt);
		entries[i++]=ret;
		if (ret) len+=strlen(ret)+2+(fmt?1:0); else fail=1;
		child=child->next;
	}
	
	// If we didn't fail, try to malloc the output string
	if (!fail) out=json_malloc(len);
	// If that fails, we fail.
	if (!out) fail=1;

	// Handle failure.
	if (fail)
	{
		for (i=0;i<numentries;i++) if (entries[i]) json_free(entries[i]);
		json_free(entries);
		return 0;
	}
	
	// Compose the output array.
	*out='[';
	ptr=out+1;*ptr=0;
	for (i=0;i<numentries;i++)
	{
		strcpy(ptr,entries[i]);ptr+=strlen(entries[i]);
		if (i!=numentries-1) {*ptr++=',';if(fmt)*ptr++=' ';*ptr=0;}
		json_free(entries[i]);
	}
	json_free(entries);
	*ptr++=']';*ptr++=0;
	return out;	
}

// Build an object from the text.
static const char *parse_object(json *item,const char *value)
{
	json *child;
	if (*value!='{')	return 0;	// not an object!
	
	item->type=json_Object;
	value=skip(value+1);
	if (*value=='}') return value+1;	// empty array.
	
	item->child=child=json_New_Item();
	value=skip(parse_string(child,skip(value)));
	if (!value) return 0;
	child->string=child->valuestring;child->valuestring=0;
	if (*value!=':') return 0;	// fail!
	value=skip(parse_value(child,skip(value+1)));	// skip any spacing, get the value.
	if (!value) return 0;
	
	while (*value==',')
	{
		json *new_item;
		if (!(new_item=json_New_Item()))	return 0; // memory fail
		child->next=new_item;new_item->prev=child;child=new_item;
		value=skip(parse_string(child,skip(value+1)));
		if (!value) return 0;
		child->string=child->valuestring;child->valuestring=0;
		if (*value!=':') return 0;	// fail!
		value=skip(parse_value(child,skip(value+1)));	// skip any spacing, get the value.		
		if (!value) return 0;
	}
	
	if (*value=='}') return value+1;	// end of array
	return 0;	// malformed.	
}

// Render an object to text.
static char *print_object(json *item,int depth,int fmt)
{
	char **entries=0,**names=0;
	char *out=0,*ptr,*ret,*str;int len=7,i=0,j;
	json *child=item->child;
	int numentries=0,fail=0;
	// Count the number of entries.
	while (child) numentries++,child=child->next;
	// Allocate space for the names and the objects
	entries=(char**)json_malloc(numentries*sizeof(char*));
	if (!entries) return 0;
	names=(char**)json_malloc(numentries*sizeof(char*));
	if (!names) {json_free(entries);return 0;}
	memset(entries,0,sizeof(char*)*numentries);
	memset(names,0,sizeof(char*)*numentries);

	// Collect all the results into our arrays:
	child=item->child;depth++;if (fmt) len+=depth;
	while (child)
	{
		names[i]=str=print_string_ptr(child->string);
		entries[i++]=ret=print_value(child,depth,fmt);
		if (str && ret) len+=strlen(ret)+strlen(str)+2+(fmt?2+depth:0); else fail=1;
		child=child->next;
	}
	
	// Try to allocate the output string
	if (!fail) out=(char*)json_malloc(len);
	if (!out) fail=1;

	// Handle failure
	if (fail)
	{
		for (i=0;i<numentries;i++) {if (names[i]) free(names[i]);if (entries[i]) free(entries[i]);}
		free(names);free(entries);
		return 0;
	}
	
	// Compose the output:
	*out='{';ptr=out+1;if (fmt)*ptr++='\n';*ptr=0;
	for (i=0;i<numentries;i++)
	{
		if (fmt) for (j=0;j<depth;j++) *ptr++='\t';
		strcpy(ptr,names[i]);ptr+=strlen(names[i]);
		*ptr++=':';if (fmt) *ptr++='\t';
		strcpy(ptr,entries[i]);ptr+=strlen(entries[i]);
		if (i!=numentries-1) *ptr++=',';
		if (fmt) *ptr++='\n';*ptr=0;
		json_free(names[i]);json_free(entries[i]);
	}
	
	json_free(names);json_free(entries);
	if (fmt) for (i=0;i<depth-1;i++) *ptr++='\t';
	*ptr++='}';*ptr++=0;
	return out;	
}

// Get Array size/item / object item.
int    json_GetArraySize(json *array)							{json *c=array->child;int i=0;while(c)i++,c=c->next;return i;}
json *json_GetArrayItem(json *array,int item)				{json *c=array->child;  while (c && item>0) item--,c=c->next; return c;}
json *json_GetObjectItem(json *object,const char *string)	{json *c=object->child; while (c && json_strcasecmp(c->string,string)) c=c->next; return c;}

// Utility for array list handling.
static void suffix_object(json *prev,json *item) {prev->next=item;item->prev=prev;}
// Utility for handling references.
static json *create_reference(json *item) {json *ref=json_New_Item();memcpy(ref,item,sizeof(json));ref->string=0;ref->type|=json_IsReference;ref->next=ref->prev=0;return ref;}

// Add item to array/object.
void   json_AddItemToArray(json *array, json *item)						{json *c=array->child;if (!c) {array->child=item;} else {while (c && c->next) c=c->next; suffix_object(c,item);}}
void   json_AddItemToObject(json *object,const char *string,json *item)	{if (item->string) json_free(item->string);item->string=json_strdup(string);json_AddItemToArray(object,item);}
void	json_AddItemReferenceToArray(json *array, json *item)						{json_AddItemToArray(array,create_reference(item));}
void	json_AddItemReferenceToObject(json *object,const char *string,json *item)	{json_AddItemToObject(object,string,create_reference(item));}

json *json_DetachItemFromArray(json *array,int which)			{json *c=array->child;while (c && which>0) c=c->next,which--;if (!c) return 0;
	if (c->prev) c->prev->next=c->next;if (c->next) c->next->prev=c->prev;if (c==array->child) array->child=c->next;c->prev=c->next=0;return c;}
void   json_DeleteItemFromArray(json *array,int which)			{json_Delete(json_DetachItemFromArray(array,which));}
json *json_DetachItemFromObject(json *object,const char *string) {int i=0;json *c=object->child;while (c && json_strcasecmp(c->string,string)) i++,c=c->next;if (c) return json_DetachItemFromArray(object,i);return 0;}
void   json_DeleteItemFromObject(json *object,const char *string) {json_Delete(json_DetachItemFromObject(object,string));}

// Replace array/object items with new ones.
void   json_ReplaceItemInArray(json *array,int which,json *newitem)		{json *c=array->child;while (c && which>0) c=c->next,which--;if (!c) return;
	newitem->next=c->next;newitem->prev=c->prev;if (newitem->next) newitem->next->prev=newitem;
	if (c==array->child) array->child=newitem; else newitem->prev->next=newitem;c->next=c->prev=0;json_Delete(c);}
void   json_ReplaceItemInObject(json *object,const char *string,json *newitem){int i=0;json *c=object->child;while(c && json_strcasecmp(c->string,string))i++,c=c->next;if(c){newitem->string=json_strdup(string);json_ReplaceItemInArray(object,i,newitem);}}

// Create basic types:
json *json_CreateNull()						{json *item=json_New_Item();item->type=json_NULL;return item;}
json *json_CreateTrue()						{json *item=json_New_Item();item->type=json_True;return item;}
json *json_CreateFalse()						{json *item=json_New_Item();item->type=json_False;return item;}
json *json_CreateNumber(double num)			{json *item=json_New_Item();item->type=json_Number;item->valuedouble=num;item->valueint=(int)num;return item;}
json *json_CreateString(const char *string)	{json *item=json_New_Item();item->type=json_String;item->valuestring=json_strdup(string);return item;}
json *json_CreateArray()						{json *item=json_New_Item();item->type=json_Array;return item;}
json *json_CreateObject()						{json *item=json_New_Item();item->type=json_Object;return item;}

// Create Arrays:
json *json_CreateIntArray(int *numbers,int count)				{int i;json *n=0,*p=0,*a=json_CreateArray();for(i=0;i<count;i++){n=json_CreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
json *json_CreateFloatArray(float *numbers,int count)			{int i;json *n=0,*p=0,*a=json_CreateArray();for(i=0;i<count;i++){n=json_CreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
json *json_CreateDoubleArray(double *numbers,int count)		{int i;json *n=0,*p=0,*a=json_CreateArray();for(i=0;i<count;i++){n=json_CreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
json *json_CreateStringArray(const char **strings,int count)	{int i;json *n=0,*p=0,*a=json_CreateArray();for(i=0;i<count;i++){n=json_CreateString(strings[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
