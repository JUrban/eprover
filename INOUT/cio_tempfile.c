/*-----------------------------------------------------------------------

File  : cio_tempfile.c

Author: Stephan Schulz

Contents

  Functions for temporary files.
 

  Copyright 1998, 1999 by the author.
  This code is released under the GNU General Public Licence.
  See the file COPYING in the main CLIB directory for details.
  Run "eprover -h" for contact information.

Changes

<1> Sat Jul 24 02:26:32 MET DST 1999
    New

-----------------------------------------------------------------------*/

/* Hack to get tempnam() without warning under Solaris 2.6 - should
   not hurt anywhere else (and might help) */
#define __EXTENSIONS__
/* Hack to get tempnam() without warning under SUSE Linux X.X - see
   above) */
#ifdef __STRICT_ANSI__
#undef __STRICT_ANSI__
#endif
#include <stdio.h> 

#include "cio_tempfile.h"


/*---------------------------------------------------------------------*/
/*                        Global Variables                             */
/*---------------------------------------------------------------------*/

static StrTree_p temp_file_store = NULL;

/*---------------------------------------------------------------------*/
/*                      Forward Declarations                           */
/*---------------------------------------------------------------------*/


/*---------------------------------------------------------------------*/
/*                         Internal Functions                          */
/*---------------------------------------------------------------------*/



/*---------------------------------------------------------------------*/
/*                         Exported Functions                          */
/*---------------------------------------------------------------------*/


/*-----------------------------------------------------------------------
//
// Function: TempFileCleanup()
//
//   Remove all temporary files.
//
// Global Variables: temp_file_store
//
// Side Effects    : Deletes files
//
/----------------------------------------------------------------------*/

void TempFileCleanup(void)
{
   while(temp_file_store)
   {
      VERBOUTARG("Removing termorary file ", temp_file_store->key);
      if(unlink(temp_file_store->key))
      {
	 sprintf(ErrStr, "Could not remove temporary file %s",
		 temp_file_store->key);	 
	 Warning(ErrStr);
      }      
      StrTreeDeleteEntry(&temp_file_store, temp_file_store->key);
   }
}

/*-----------------------------------------------------------------------
//
// Function: TempFileRegister()
//
//   Register a file as temporary and to remove at exit.
//
// Global Variables: temp_file_store
//
// Side Effects    : Memory operations
//
/----------------------------------------------------------------------*/

void TempFileRegister(char *name)
{
   StrTree_p res;
   IntOrP tmp;

   tmp.p_val = NULL;     
   res = StrTreeStore(&temp_file_store, name, tmp, tmp);
   assert(res);
}


/*-----------------------------------------------------------------------
//
// Function: TempFileName()
//
//   Allocate and register a new temporary file name. The caller has
//   to free  the name!
//
// Global Variables: temp_file_store
//
// Side Effects    : Memory operations
//
/----------------------------------------------------------------------*/

char* TempFileName(void)
{
   char *name = tempnam(NULL, "epr_");
   
   if(!name)
   {
      Error("Could not create valid temporary file name", FILE_ERROR);
   }   
   TempFileRegister(name);
   
   return name;
}


/*-----------------------------------------------------------------------
//
// Function: TempFileCreate()
//
//   Create a temporary file storing the data from source. Return name
//   of the created file.
//
// Global Variables: -
//
// Side Effects    : Writes file, reads input, allocates new name!
//
/----------------------------------------------------------------------*/

char* TempFileCreate(FILE* source)
{
   char* name;
   FILE* out;
   int c;

   name = TempFileName();
   out = OutOpen(name);

   while((c = getc(source))!= EOF)
   {
      putc(c, out);
   }
   OutClose(out);
   return name;
}


/*-----------------------------------------------------------------------
//
// Function: TempFileRemove()
//
//   Remove a temporary file.
//
// Global Variables: -
//
// Side Effects    : Removes file.
//
/----------------------------------------------------------------------*/

void TempFileRemove(char* name)
{
   bool res;

   if(unlink(name)!=0)
   {
      TmpErrno = errno;
      SysError("Could not remove temporary file", SYS_ERROR);
   }
   res = StrTreeDeleteEntry(&temp_file_store, name);
   assert(res);
}


/*-----------------------------------------------------------------------
//
// Function: CLStateCreateTempFiles()
//
//   For each element in state->argv, check if it is "-". If yes,
//   create a temp file for it and replace "-" with the temp file
//   name. Returns a stack with pointers to all temp file names.
//
// Global Variables: -
//
// Side Effects    : Lots...see above.
//
/----------------------------------------------------------------------*/

PStack_p CLStateCreateTempFiles(CLState_p state)
{
   PStack_p stack = PStackAlloc();
   int i;
   
   for(i=0; state->argv[i]; i++)
   {
      if(strcmp(state->argv[i], "-") == 0)
      {
	 PStackPushP(stack, state->argv[i]);
	 PStackPushInt(stack, i);
	 state->argv[i] = TempFileCreate(stdin);
	 PStackPushP(stack, state->argv[i]);
      }
   }   
   return stack;
}


/*-----------------------------------------------------------------------
//
// Function: CLStateDestroyTempFiles()
//
//   Given a stack of temp file names, positions and original file
//   names, remove all files, free the file names and restore the
//   original state argv.
//
// Global Variables: -
//
// Side Effects    : Only ;-)
//
/----------------------------------------------------------------------*/

void CLStateDestroyTempFiles(CLState_p state, PStack_p files)
{
   char *arg;
   long i;

   while(!PStackEmpty(files))
   {
      arg = PStackPopP(files);
      TempFileRemove(arg);
      FREE(arg);
      i = PStackPopInt(files);
      arg = PStackPopP(files);
      state->argv[i] = arg;
   }
   PStackFree(files);
}


/*---------------------------------------------------------------------*/
/*                        End of File                                  */
/*---------------------------------------------------------------------*/


