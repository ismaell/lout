/*@z06.c:Parser:PushObj(), PushToken(), etc.@*********************************/
/*                                                                           */
/*  THE LOUT DOCUMENT FORMATTING SYSTEM (VERSION 3.02)                       */
/*  COPYRIGHT (C) 1994 Jeffrey H. Kingston                                   */
/*                                                                           */
/*  Jeffrey H. Kingston (jeff@cs.su.oz.au)                                   */
/*  Basser Department of Computer Science                                    */
/*  The University of Sydney 2006                                            */
/*  AUSTRALIA                                                                */
/*                                                                           */
/*  This program is free software; you can redistribute it and/or modify     */
/*  it under the terms of the GNU General Public License as published by     */
/*  the Free Software Foundation; either version 1, or (at your option)      */
/*  any later version.                                                       */
/*                                                                           */
/*  This program is distributed in the hope that it will be useful,          */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/*  GNU General Public License for more details.                             */
/*                                                                           */
/*  You should have received a copy of the GNU General Public License        */
/*  along with this program; if not, write to the Free Software              */
/*  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/*                                                                           */
/*  FILE:         z06.c                                                      */
/*  MODULE:       Parser                                                     */
/*  EXTERNS:      InitParser(), Parse()                                      */
/*                                                                           */
/*****************************************************************************/
#include "externs"
#define	LEFT_ASSOC	0
#define	RIGHT_ASSOC	1

#define	PREV_OP		0		/* means an operator was previous    */
#define	PREV_OBJ	1		/* prev was object not ending in RBR */
#define	PREV_RBR	2		/* prev was object ending in RBR     */

static	OBJECT		cross_name;	/* name of the cr database   */


#define	MAX_STACK	50			/* size of parser stacks     */
static	OBJECT		obj_stack[MAX_STACK];	/* stack of objects          */
static	int		otop = -1;		/* top of obj_stack          */
static	OBJECT		tok_stack[MAX_STACK];	/* stack of tokens           */
static	int		ttop = -1;		/* top of tok_stack          */
static	BOOLEAN		debug_now = FALSE;	/* TRUE when want to debug   */


/*****************************************************************************/
/*                                                                           */
/*  PushObj(x)                                                               */
/*  PushToken(t)                                                             */
/*  OBJECT PopObj()                                                          */
/*  OBJECT PopToken()                                                        */
/*  OBJECT TokenTop                                                          */
/*  OBJECT ObjTop                                                            */
/*                                                                           */
/*  Push and pop from the object and token stacks; examine top item.         */
/*                                                                           */
/*****************************************************************************/

#define PushObj(x)							\
{ zz_hold = x;								\
  if( ++otop < MAX_STACK ) obj_stack[otop] = zz_hold;			\
  else Error(6, 1, "expression is too deeply nested",			\
    FATAL, &fpos(obj_stack[otop-1]));					\
}

#define PushToken(t)							\
{ if( ++ttop < MAX_STACK ) tok_stack[ttop] = t;				\
  else Error(6, 2, "expression is too deeply nested",			\
	 FATAL, &fpos(tok_stack[ttop-1]));				\
}

#define PopObj()	obj_stack[otop--]
#define PopToken()	tok_stack[ttop--]
#define	TokenTop	tok_stack[ttop]
#define	ObjTop		obj_stack[otop]


/*@::DebugStacks(), InsertSpace()@********************************************/
/*                                                                           */
/*  DebugStacks()                                                            */
/*                                                                           */
/*  Print debug output of the stacks state                                   */
/*                                                                           */
/*****************************************************************************/

#if DEBUG_ON
static DebugStacks(initial_ttop, obj_prev)
int initial_ttop;  int obj_prev;
{ int i;
  debug3(DOP, D, "  obj_prev: %s; otop: %d; ttop: %d",
    obj_prev == PREV_OP ? "PREV_OP" : obj_prev == PREV_OBJ ? "PREV_OBJ" :
    obj_prev == PREV_RBR ? "PREV_RBR" : "???", otop, ttop);
  for( i = 0;  i <= otop; i++ )
    debug3(DOP, D, "  obj[%d] = (%s) %s", i,
       Image(type(obj_stack[i])), EchoObject(obj_stack[i]));
  for( i = 0;  i <= ttop;  i++ )
  { if( i == initial_ttop+1 ) debug0(DOP, D, "  $");
    debug3(DOP, D, "  tok[%d] = %s (precedence %d)", i,
      type(tok_stack[i]) == CLOSURE ?
	SymName(actual(tok_stack[i])) : Image(type(tok_stack[i])),
      precedence(tok_stack[i]));
  }
}
#endif


/*****************************************************************************/
/*                                                                           */
/*  InsertSpace(t)                                                           */
/*                                                                           */
/*  Add any missing catenation operator in front of token t.                 */
/*                                                                           */
/*****************************************************************************/

#define InsertSpace(t)							\
if( obj_prev )								\
{ int typ, prec;							\
  if( hspace(t) + vspace(t) > 0 )					\
    typ = TSPACE, prec = ACAT_PREC;					\
  else if( type(t) == LBR || obj_prev == PREV_RBR )			\
    typ = TJUXTA, prec = ACAT_PREC;					\
  else									\
    typ = TJUXTA, prec = JUXTA_PREC;					\
  debugcond1(DOP, DD, debug_now, "[ InsertSpace(%s)", Image(typ));	\
  while( obj_prev && precedence(TokenTop) >= prec )			\
    obj_prev = Reduce();						\
  if( obj_prev )							\
  { tmp = New(typ);  precedence(tmp) = prec;				\
    vspace(tmp) = vspace(t);  hspace(tmp) = hspace(t);			\
    mark(gap(tmp)) = FALSE;  join(gap(tmp)) = TRUE;			\
    FposCopy(fpos(tmp), fpos(t));					\
    PushToken(tmp);							\
  }									\
  debugcond0(DOP, DD, debug_now, "] end InsertSpace()");		\
} /* end InsertSpace */


/*@::Shift(), ShiftObj()@*****************************************************/
/*                                                                           */
/*  static Shift(t, prec, rassoc, leftpar, rightpar)                         */
/*  static ShiftObj(t)                                                       */
/*                                                                           */
/*  Shift token or object t onto the stacks; it has the attributes shown.    */
/*                                                                           */
/*****************************************************************************/

#define Shift(t, prec, rassoc, leftpar, rightpar)			\
{ debugcond5(DOP, DD, debug_now, "[ Shift(%s, %d, %s, %s, %s)",		\
    Image(type(t)), prec, rassoc ? "rightassoc" : "leftassoc",		\
      leftpar ? "lpar" : "nolpar", rightpar ? "rpar" : "norpar");	\
  if( leftpar )								\
  { for(;;)								\
    { if( !obj_prev )							\
      {	PushObj( MakeWord(WORD, STR_EMPTY, &fpos(t)) );			\
	obj_prev = PREV_OBJ;						\
      }									\
      else if( precedence(TokenTop) >= prec + rassoc )			\
      { obj_prev = Reduce();						\
	if( ttop == initial_ttop )					\
	{ *token = t;							\
	  debugcond0(DOP, D, debug_now,					\
	    "] ] end Shift() and Parse(); stacks are:");		\
	  ifdebugcond(DOP, D, debug_now,				\
	    DebugStacks(initial_ttop, obj_prev));			\
	  return PopObj();						\
	}								\
      }									\
      else break;							\
    }									\
  }									\
  else InsertSpace(t);							\
  PushToken(t);								\
  if( rightpar )  obj_prev = FALSE;					\
  else									\
  { obj_prev = Reduce(); 						\
    if( ttop == initial_ttop )						\
    { *token = nil;							\
      debugcond0(DOP, D, debug_now,					\
	"] ] end Shift and Parse; stacks are:");			\
      ifdebugcond(DOP, D, debug_now,					\
	DebugStacks(initial_ttop, obj_prev));				\
      return PopObj();							\
    }									\
  }									\
  debugcond0(DOP, DD, debug_now, "] end Shift()");			\
} /* end Shift */


#define ShiftObj(t, new_obj_prev)					\
{ debugcond1(DOP, DD, debug_now, "[ ShiftObj(%s)", Image(type(t)));	\
  InsertSpace(t);							\
  PushObj(t);								\
  obj_prev = new_obj_prev;						\
  debugcond0(DOP, DD, debug_now, "] end ShiftObj()");			\
}

/*@::Reduce()@****************************************************************/
/*                                                                           */
/*  static Reduce()                                                          */
/*                                                                           */
/*  Perform a single reduction of the stacks.                                */
/*                                                                           */
/*****************************************************************************/

static BOOLEAN Reduce()
{ OBJECT p1, p2, p3, s1, s2, tmp;
  OBJECT op;  int obj_prev;
  debugcond0(DOP, DD, debug_now, "[ Reduce()");
  /* ifdebugcond(DOP, DD, debug_now, DebugStacks(0, TRUE)); */

  op = PopToken();
  obj_prev = PREV_OBJ;
  switch( type(op) )
  {

    case GSTUB_INT:
    case GSTUB_EXT:
    
	debug0(DGT, D, "calling TransferEnd( PopObj() ) from Reduce()");
	TransferEnd( PopObj() );
	PushObj(New(NULL_CLOS));
	Dispose(op);
	break;


    case GSTUB_NONE:

	PushObj(New(NULL_CLOS));
	Dispose(op);
	break;


    case NULL_CLOS:
    case CROSS:
    case ONE_ROW:
    case ONE_COL:
    case WIDE:
    case HIGH:
    case HSHIFT:
    case VSHIFT:
    case HSCALE:
    case VSCALE:
    case SCALE:
    case HCONTRACT:
    case VCONTRACT:
    case HEXPAND:
    case VEXPAND:
    case PADJUST:
    case HADJUST:
    case VADJUST:
    case ROTATE:
    case CASE:
    case YIELD:
    case BACKEND:
    case XCHAR:
    case FONT:
    case SPACE:
    case BREAK:
    case COLOUR:
    case LANGUAGE:
    case CURR_LANG:
    case NEXT:
    case TAGGED:
    case INCGRAPHIC:
    case SINCGRAPHIC:
    case GRAPHIC:
    case OPEN:

	if( has_rpar(actual(op)) )
	{ s2 = PopObj();
	  Link(op, s2);
	}
	if( has_lpar(actual(op)) )
	{ s1 = PopObj();
	  Link(Down(op), s1);
	  if( type(op) == CROSS && type(s1) != CLOSURE )
	    Error(6, 3, "left parameter of %s is not a symbol (or not visible)",
	      WARN, &fpos(s1), KW_CROSS);
	}
	PushObj(op);
	break;


    case CLOSURE:
    
	if( has_rpar(actual(op)) )
	{ s2 = New(PAR);
	  tmp = PopObj();
	  Link(s2, tmp);
	  FposCopy(fpos(s2), fpos(tmp));
	  actual(s2) = ChildSym(actual(op), RPAR);
	  Link(op, s2);
	}
	if( has_lpar(actual(op)) )
	{ s1 = New(PAR);
	  tmp = PopObj();
	  Link(s1, tmp);
	  FposCopy(fpos(s1), fpos(tmp));
	  actual(s1) = ChildSym(actual(op), LPAR);
	  Link(Down(op), s1);
	}
	PushObj(op);
	break;


    case LBR:
    
	Error(6, 4, "unmatched %s (inserted %s)", WARN, &fpos(op),
	  KW_LBR, KW_RBR);
	Dispose(op);
	obj_prev = PREV_RBR;
	break;


    case BEGIN:
    
	Error(6, 5, "Reduce: unmatched %s", INTERN, &fpos(op), KW_BEGIN);
	break;


    case RBR:
    
	if( type(TokenTop) == LBR )
	{ /* *** FposCopy(fpos(ObjTop), fpos(TokenTop)); *** */
	  Dispose( PopToken() );
	}
	else if( type(TokenTop) == BEGIN )
	{ if( file_num(fpos(TokenTop)) > 0 )
	    Error(6, 6, "unmatched %s; inserted %s at%s (after %s)",
	      WARN, &fpos(op), KW_RBR, KW_LBR,
	      EchoFilePos(&fpos(TokenTop)), KW_BEGIN);
	  else
	    Error(6, 7, "unmatched %s not enclosed in anything",
	      FATAL, &fpos(op), KW_RBR);
	}
	else Error(6, 8, "Reduce: unmatched %s", INTERN, &fpos(op), KW_RBR);
	Dispose(op);
	obj_prev = PREV_RBR;
	break;


    case END:
    
	if( type(TokenTop) != BEGIN )
	  Error(6, 9, "Reduce: unmatched %s", INTERN, &fpos(op), KW_END);
	else
	{ if( actual(op) != actual(TokenTop) )
	    if( actual(op) == StartSym )
	      Error(6, 10, "%s %s appended at end of file to match %s at%s",
		WARN, &fpos(op), KW_END, SymName(actual(TokenTop)),
		KW_BEGIN, EchoFilePos(&fpos(TokenTop)) );
	    else if( actual(op) == nil )
	      Error(6, 11, "%s replaced by %s %s to match %s at%s",
	        WARN, &fpos(op), KW_END, KW_END,
		actual(TokenTop) == nil ? AsciiToFull("??") :
		  SymName(actual(TokenTop)),
		KW_BEGIN, EchoFilePos(&fpos(TokenTop)) );
	    else
	      Error(6, 12, "%s %s replaced by %s %s to match %s at%s",
	        WARN, &fpos(op), KW_END, SymName(actual(op)),
		KW_END, SymName(actual(TokenTop)),
		KW_BEGIN, EchoFilePos(&fpos(TokenTop)) );
	  Dispose( PopToken() );
	}
	Dispose(op);
	obj_prev = PREV_RBR;
	break;


    case GAP_OBJ:

	p1 = PopObj();
	Link(op, p1);
	PushObj(op);
	obj_prev = PREV_OP;
	break;


    case VCAT:
    case HCAT:
    case ACAT:
    
	p3 = PopObj();  p2 = PopObj();  p1 = PopObj();
	if( type(p1) == type(op) )  Dispose(op);
	else
	{ Link(op, p1);
	  p1 = op;
	}
	Link(p1, p2);
	Link(p1, p3);
	PushObj(p1);
	break;


    case TSPACE:
    case TJUXTA:

	p2 = PopObj();  p1 = PopObj();
	if( type(p1) != ACAT )
	{ tmp = New(ACAT);
	  Link(tmp, p1);
	  FposCopy(fpos(tmp), fpos(p1));
	  p1 = tmp;
	}
	type(op) = GAP_OBJ;
	Link(p1, op);
	Link(p1, p2);
	PushObj(p1);
	break;


    default:
    
	Error(6, 13, "Reduce: %s", INTERN, &fpos(op), Image(type(op)) );
	break;

  } /* end switch */
  debugcond1(DOP, DD, debug_now, "] end Reduce(), returning %s",
    obj_prev == PREV_OP ? "PREV_OP" : obj_prev == PREV_OBJ ? "PREV_OBJ" :
    obj_prev == PREV_RBR ? "PREV_RBR" : "???");
  return obj_prev;
} /* end Reduce */


/*@::SetScope(), InitParser()@************************************************/
/*                                                                           */
/*  SetScope(env, count)                                                     */
/*                                                                           */
/*  Push scopes required to parse object whose environment is env.           */
/*  Add to *count the number of scope pushes made.                           */
/*                                                                           */
/*****************************************************************************/

SetScope(env, count)
OBJECT env;  int *count;
{ OBJECT link, y, yenv;  BOOLEAN visible_only;
  debugcond2(DOP,D, debug_now, "[ SetScope(%s, %d)", EchoObject(env), *count);
  assert( env != nil && type(env) == ENV, "SetScope: type(env) != ENV!" );
  if( Down(env) != env )
  { Child(y, Down(env));
    assert( LastDown(y) != y, "SetScope: LastDown(y)!" );
    link = LastDown(env) != Down(env) ? LastDown(env) : LastDown(y);
    Child(yenv, link);
    assert( type(yenv) == ENV, "SetScope: type(yenv) != ENV!" );
    SetScope(yenv, count);
    visible_only = (use_invocation(actual(y)) != nil); /* i.e. from @Use clause */
    PushScope(actual(y), FALSE, visible_only);  (*count)++;
  }
  debugcond1(DOP, D, debug_now, "] SetScope returning, count = %d", *count);
} /* end SetScope */


/*****************************************************************************/
/*                                                                           */
/*  InitParser()                                                             */
/*                                                                           */
/*  Initialise the parser to contain just GstubExt.                          */
/*  Remember cross_db, the name of the cross reference database, for Parse.  */
/*                                                                           */
/*****************************************************************************/

InitParser(cross_db)
FULL_CHAR *cross_db;
{ if( StringLength(cross_db) >= MAX_WORD )
    Error(6, 14, "cross reference database file name %s is too long",
      FATAL, no_fpos, cross_db);
  cross_name = MakeWord(WORD, cross_db, no_fpos);
  PushToken( NewToken(GSTUB_EXT, no_fpos, 0, 0, DEFAULT_PREC, StartSym) );
} /* end InitParser */


/*@::ParseEnvClosure()@*******************************************************/
/*                                                                           */
/*  static OBJECT ParseEnvClosure(t, encl)                                   */
/*                                                                           */
/*  Parse an object which is a closure with environment.  Consume the        */
/*  concluding @Clos.                                                        */
/*                                                                           */
/*****************************************************************************/

static OBJECT ParseEnvClosure(t, encl)
OBJECT t, encl;
{ OBJECT env, res, y;  int count, i;
  debugcond0(DOP, DDD, debug_now, "ParseEnvClosure(t, encl)");
  assert( type(t) == ENV, "ParseEnvClosure: type(t) != ENV!" );
  env = t;  t = LexGetToken();
  while( type(t) != CLOS )  switch( type(t) )
  {
    case LBR:	count = 0;
		SetScope(env, &count);
		y = Parse(&t, encl, FALSE, FALSE);
		if( type(y) != CLOSURE )
		  Error(6, 15, "syntax error in cross reference database",
		    FATAL, &fpos(y));
		for( i = 1;  i <= count;  i++ )  PopScope();
		AttachEnv(env, y);
		debug0(DCR, DD, "  calling SetEnv from ParseEnvClosure (a)");
		env = SetEnv(y, nil);
		t = LexGetToken();
		break;

    case ENV:	y = ParseEnvClosure(t, encl);
		debug0(DCR, DD, "  calling SetEnv from ParseEnvClosure (b)");
		env = SetEnv(y, env);
		t = LexGetToken();
		break;

    default:	Error(6, 16, "error in cross reference database",
		  FATAL, &fpos(t));
		break;
  }
  Dispose(t);
  if( Down(env) == env || Down(env) != LastDown(env) )
    Error(6, 17, "error in cross reference database", FATAL, &fpos(env));
  Child(res, Down(env));
  DeleteNode(env);
  debugcond1(DOP, DDD, debug_now, "ParseEnvClosure ret. %s", EchoObject(res));
  assert( type(res) == CLOSURE, "ParseEnvClosure: type(res) != CLOSURE!" );
  return res;
} /* end ParseEnvClosure */


/*@::Parse()@*****************************************************************/
/*                                                                           */
/*  OBJECT Parse(token, encl, defs_allowed, transfer_allowed)                */
/*                                                                           */
/*  Parse input tokens, beginning with *token, looking for an object of the  */
/*  form { ... } or @Begin ... @End <sym>, and return the object.            */
/*  The parent definition is encl, and scope has been set appropriately.     */
/*  Parse reads up to and including the last token of the object             */
/*  (the right brace or <sym>), and returns nil in *token.                   */
/*                                                                           */
/*  If defs_allowed == TRUE, there may be local definitions in the object.   */
/*  In this case, encl is guaranteed to be the enclosing definition.         */
/*                                                                           */
/*  If transfer_allowed == TRUE, the parser may transfer components to the   */
/*  galley handler as they are read.                                         */
/*                                                                           */
/*  Note: the lexical analyser returns "@End \Input" at end of input, so the */
/*  parser does not have to handle end of input separately.                  */
/*                                                                           */
/*****************************************************************************/

OBJECT Parse(token, encl, defs_allowed, transfer_allowed)
OBJECT *token, encl;  BOOLEAN defs_allowed, transfer_allowed;
{ OBJECT t, x, tmp, xsym, env, y, link, res;
  int i, initial_ttop = ttop;  int obj_prev;

  debugcond4(DOP, D, debug_now, "[ Parse(%s, %s, %s, %s)", EchoToken(*token),
      SymName(encl), bool(defs_allowed), bool(transfer_allowed));
  assert( type(*token) == LBR || type(*token) == BEGIN, "Parse: *token!" );

  obj_prev = PREV_OP;
  Shift(*token, precedence(*token), 0, FALSE, TRUE);
  t = LexGetToken();
  if( defs_allowed )
  { ReadDefinitions(&t, encl, LOCAL);
    if( encl == StartSym ) /* transition point from defs to content */
    {
      /* turn on debugging now */
      debug_now = TRUE;
      debugcond4(DOP, D, debug_now, "[ Parse (first) (%s, %s, %s, %s)",
	EchoToken(*token), SymName(encl), bool(defs_allowed),
	bool(transfer_allowed));

      /* if error in definitions, stop now */
      if( ErrorSeen() )
	Error(6, 18, "exiting now", FATAL, &fpos(t));

      /* load cross-references from previous run, open new cross refs */
      if( AllowCrossDb )
      {	NewCrossDb = DbCreate(MakeWord(WORD, string(cross_name), no_fpos));
	OldCrossDb = DbLoad(cross_name, SOURCE_PATH, FALSE, nil);
      }
      else OldCrossDb = NewCrossDb = nil;

      /* tidy up and possibly print symbol table */
      FlattenUses();
      ifdebug(DST, D, DebugObject(StartSym));

      /* read @Use, @Database, and @Prepend commands and construct env */
      env = New(ENV);
      for(;;)
      {	if( type(t) == USE )
	{
	  OBJECT crs, res_env;  STYLE style;
	  Dispose(t);  t = LexGetToken();
	  if( type(t) != LBR )
	    Error(6, 19, "%s expected after %s", FATAL, &fpos(t),KW_LBR,KW_USE);
	  debug0(DOP, DD, "  Parse() calling Parse for @Use clause");
	  y = Parse(&t, encl, FALSE, FALSE);
	  if( type(y) == CROSS )
	  { OBJECT z;
	    Child(z, Down(y))
	    if( type(z) == CLOSURE )
	    { y = CrossExpand(y, env, &style, FALSE, &crs, &res_env);
	      AttachEnv(res_env, y);
	      debug0(DCR, DD, "  calling SetEnv from Parse (a)");
	      env = SetEnv(y, env);
	    }
	    else Error(6, 21, "invalid parameter of %s", FATAL, &fpos(y), KW_USE);
	  }
	  else if( type(y) == CLOSURE )
	  { if( use_invocation(actual(y)) != nil )
	      Error(6, 20, "symbol %s occurs in two %s clauses",
		FATAL, &fpos(y), SymName(actual(y)), KW_USE);
	    use_invocation(actual(y)) = y;
	    AttachEnv(env, y);
	    debug0(DCR, DD, "  calling SetEnv from Parse (b)");
	    env = SetEnv(y, nil);
	  }
	  else Error(6, 21, "invalid parameter of %s", FATAL, &fpos(y), KW_USE);
	  PushScope(actual(y), FALSE, TRUE);
	  t = LexGetToken();
        }
	else if( type(t) == PREPEND || type(t) == SYS_PREPEND )
	{ ReadPrependDef(type(t), encl);
	  Dispose(t);
	  t = LexGetToken();
	}
	else if( type(t) == DATABASE || type(t) == SYS_DATABASE )
	{ ReadDatabaseDef(type(t), encl);
	  Dispose(t);
	  t = LexGetToken();
	}
	else break;
      }
      TransferInit(env);
    }
  }

  for(;;)
  { 
    debugcond0(DOP, DD, debug_now, "");
    ifdebugcond(DOP, DD, debug_now, DebugStacks(0, obj_prev));
    debugcond0(DOP, DD, debug_now, "");
    debugcond2(DOP, DD, debug_now, ">> %s (precedence %d)", EchoToken(t), precedence(t));

    switch( type(t) )
    {

      case WORD:
      
	if( string(t)[0] == CH_SYMSTART )
	  Error(6, 22, "symbol %s unknown or misspelt",
	    WARN, &fpos(t), string(t));
	ShiftObj(t, PREV_OBJ);
	t = LexGetToken();
	break;


      case QWORD:
      
	ShiftObj(t, PREV_OBJ);
	t = LexGetToken();
	break;


      case VCAT:
      case HCAT:
      case ACAT:
      
	/* clean up left context */
	Shift(t, precedence(t), LEFT_ASSOC, TRUE, TRUE);

	/* invoke transfer subroutines if appropriate */
	if( type(t) == VCAT && !has_join(actual(t))
		&& type(tok_stack[ttop-2]) == GSTUB_EXT )
	{ TransferComponent( PopObj() );
	  tmp = New(NULL_CLOS);
	  FposCopy( fpos(tmp), fpos(t) );
	  PushObj(tmp);
	}

	/* push GAP_OBJ token, to cope with 3 parameters */
	x = New(GAP_OBJ);
	mark(gap(x)) = has_mark(actual(t));
	join(gap(x)) = has_join(actual(t));
	precedence(x) = GAP_PREC;
	FposCopy( fpos(x), fpos(t) );
	Shift(x, GAP_PREC, LEFT_ASSOC, FALSE, TRUE);

	/* if op is followed by space, insert {} */
	t = LexGetToken();
	if( hspace(t) + vspace(t) > 0 )
	{ ShiftObj(MakeWord(WORD, STR_EMPTY, &fpos(x)), PREV_OBJ);
	}
	break;


      case CROSS:
      case NULL_CLOS:
      case ONE_COL:
      case ONE_ROW:
      case WIDE:
      case HIGH:
      case HSHIFT:
      case VSHIFT:
      case HSCALE:
      case VSCALE:
      case SCALE:
      case HCONTRACT:
      case VCONTRACT:
      case HEXPAND:
      case VEXPAND:
      case PADJUST:
      case HADJUST:
      case VADJUST:
      case ROTATE:
      case CASE:
      case YIELD:
      case BACKEND:
      case XCHAR:
      case FONT:
      case SPACE:
      case BREAK:
      case COLOUR:
      case LANGUAGE:
      case CURR_LANG:
      case NEXT:
      case TAGGED:
      case INCGRAPHIC:
      case SINCGRAPHIC:
      case GRAPHIC:

	/* clean up left context of t (these ops are all right associative) */
	Shift(t, precedence(t), RIGHT_ASSOC,
		has_lpar(actual(t)), has_rpar(actual(t)));
	t = LexGetToken();
	break;


      case UNEXPECTED_EOF:

	Error(6, 23, "unexpected end of input", FATAL, &fpos(t));
	break;


      case BEGIN:
      
	if( actual(t) == nil )
	{ Error(6, 24, "%s replaced by %s", WARN, &fpos(t), KW_BEGIN, KW_LBR);
	  type(t) = LBR;
	}
	/* NB NO BREAK! */


      case LBR:
      
	Shift(t, LBR_PREC, 0, FALSE, TRUE);
	t = LexGetToken();
	break;


      case END:
      
	if( actual(t) == nil )  /* haven't sought following symbol yet */
	{ x = LexGetToken();
	  if( type(x) == CLOSURE )
	  { actual(t) = actual(x);
	    Dispose(x);
	    x = nil;
	  }
	  else if( type(x) == WORD && string(x)[0] == CH_SYMSTART )
	  { Error(6, 25, "unknown or misspelt symbol %s after %s deleted",
	      WARN, &fpos(x), string(x), KW_END);
	    actual(t) = nil;
	    Dispose(x);
	    x = nil;
	  }
	  else
	  { Error(6, 26, "symbol expected after %s", WARN, &fpos(x), KW_END);
	    actual(t) = nil;
	  }
	}
	else x = nil;
	Shift(t, precedence(t), 0, TRUE, FALSE);
	/* *** now doing this inside Shift() ***
	if( ttop == initial_ttop )
	{
	  *token = x;
	  debugcond0(DOP, D, debug_now, "] end Parse(); final stacks are:");
	  ifdebugcond(DOP, D, debug_now, DebugStacks(initial_ttop, obj_prev));
	  return PopObj();
	}
	*** */
	t = (x != nil) ? x : LexGetToken();
	break;


      case RBR:
      
	Shift(t, precedence(t), 0, TRUE, FALSE);
	/* *** now doing this inside Shift() ***
	if( ttop == initial_ttop )
	{
	  *token = nil;
	  debugcond0(DOP, D, debug_now, "] end Parse(); final stacks are:");
	  ifdebugcond(DOP, D, debug_now, DebugStacks(initial_ttop, obj_prev));
	  return PopObj();
	}
	*** */
	t = LexGetToken();
	break;
				

      case USE:
      case PREPEND:
      case SYS_PREPEND:
      case DATABASE:
      case SYS_DATABASE:
      
	Error(6, 27, "%s symbol out of place",
	  FATAL, &fpos(t), SymName(actual(t)));
	break;


      case ENV:
      
	/* only occurs in cross reference databases */
	res = ParseEnvClosure(t, encl);
	ShiftObj(res, PREV_OBJ);
	t = LexGetToken();
	break;


      case LUSE:

	/* only occurs in cross-reference databases */
	/* copy invocation from use_invocation(xsym), don't read it */
	Dispose(t);  t = LexGetToken();
	if( type(t) != CLOSURE )
	  Error(6, 28, "symbol expected following %s", FATAL,&fpos(t),KW_LUSE);
	xsym = actual(t);
	if( use_invocation(xsym) == nil )
	  Error(6, 29, "%s clause(s) changed from previous run",
	    FATAL, &fpos(t), KW_USE);
	x = CopyObject(use_invocation(xsym), no_fpos);
	for( link = LastDown(x);  link != x;  link = PrevDown(link) )
	{ Child(y, link);
	  if( type(y) == ENV )
	  { DeleteLink(link);
	    break;
	  }
	}
	ShiftObj(x, PREV_OBJ);
	t = LexGetToken();
	break;


      case LVIS:
      
	/* only occurs in cross-reference databases */
	SuppressVisible();
	Dispose(t);  t = LexGetToken();
	UnSuppressVisible();
	if( type(t) != CLOSURE )
	  Error(6, 30, "symbol expected following %s", FATAL,&fpos(t),KW_LVIS);
	/* NB NO BREAK! */


      case CLOSURE:
      
	x = t;  xsym = actual(x);

	/* look ahead one token, which could be an NPAR */
	PushScope(xsym, TRUE, FALSE);
	t = LexGetToken();
	PopScope();

	/* if x starts a cross-reference, make it a CLOSURE */
	if( type(t) == CROSS )
	{ ShiftObj(x, PREV_OBJ);
	  break;
	}

	/* clean up left context of x */
	Shift(x,precedence(x),right_assoc(xsym),has_lpar(xsym),has_rpar(xsym));

	/* update uses relation if required */
	if( encl != StartSym && encl != nil )
	{ if( !has_target(xsym) )  InsertUses(encl, xsym);
	  else uses_galley(encl) = TRUE;
	}

	/* read named parameters */
	while( type(t) == CLOSURE && enclosing(actual(t)) == xsym
				       && type(actual(t)) == NPAR )
	{	
	  /* check syntax and attach the named parameter to x */
	  OBJECT new_par = t;
	  t = LexGetToken();
	  if( type(t) != LBR )
	  { Error(6, 31, "%s must follow named parameter %s",
	      WARN, &fpos(new_par), KW_LBR, SymName(actual(new_par)));
	    Dispose(new_par);
	    break;
	  }

	  /* read the body of the named parameter */
	  PushScope(actual(new_par), FALSE, FALSE);
	  tmp = Parse(&t, encl, FALSE, FALSE);
	  type(new_par) = PAR;
	  Link(new_par, tmp);

	  /* check that new_par has not already occurred, then link it to x */
	  for( link = Down(x);  link != x;  link = NextDown(link) )
	  { Child(y, link);
	    assert( type(y) == PAR, "Parse: type(y) != PAR!" );
	    if( actual(new_par) == actual(y) )
	    { Error(6, 32, "named parameter %s of %s appears twice", WARN,
		&fpos(new_par), SymName(actual(new_par)), SymName(actual(x)));
	      DisposeObject(new_par);
	      new_par = nil;
	      break;
	    }
	  }
	  if( new_par != nil )  Link(x, new_par);

	  /* get next token, possibly another NPAR */
	  PopScope();
	  PushScope(xsym, TRUE, FALSE);	 /* allow NPARs only */
	  if( t == nil )  t = LexGetToken();
	  PopScope();

	} /* end while */

	/* record symbol name in BEGIN following, if any */
	if( type(t) == BEGIN )
	{ if( !has_rpar(xsym) )
	    Error(6, 33, "%s out of place here (%s has no right parameter)",
	      WARN, &fpos(x), KW_BEGIN, SymName(xsym));
	  else actual(t) = xsym;
	}

	/* if x can be transferred, do so */
	if( transfer_allowed && has_target(xsym) &&
	    !has_key(xsym) && filter(xsym) == nil )
	{   
	  if( !has_rpar(xsym) || uses_count(ChildSym(xsym, RPAR)) <= 1 )
	  {
	    debug1(DGT, DD, "examining transfer of %s", SymName(xsym));
	    ifdebug(DGT, DD, DebugStacks(initial_ttop, obj_prev));
	    i = has_rpar(xsym) ? ttop -1 : ttop;
	    while( is_cat_op(type(tok_stack[i])) )   i--;
	    if( (type(tok_stack[i])==LBR || type(tok_stack[i])==BEGIN)
		  && type(tok_stack[i-1]) == GSTUB_EXT )
	    {
	      /* at this point it is likely that x is transferable */
	      if( has_rpar(xsym) )
	      { tmp = New(CLOSURE);
		actual(tmp) = InputSym;
		FposCopy( fpos(tmp), fpos(t) );
		ShiftObj(tmp, PREV_OBJ);
		obj_prev = Reduce();
	      }
	      x = PopObj();
	      x = TransferBegin(x);
	      if( type(x) == CLOSURE )	/* failure: unReduce */
	      {	if( has_rpar(xsym) )
		{ Child(tmp, LastDown(x));
		  assert(type(tmp)==PAR && type(actual(tmp))==RPAR,
				"Parse: cannot undo rpar" );
		  DisposeChild(LastDown(x));
		  if( has_lpar(xsym) )
		  { Child(tmp, Down(x));
		    assert(type(tmp)==PAR && type(actual(tmp))==LPAR,
				"Parse: cannot undo lpar" );
		    Child(tmp, Down(tmp));
		    PushObj(tmp);
		    DeleteLink(Up(tmp));
		    DisposeChild(Down(x));
		  }
		  PushToken(x);  obj_prev = PREV_OP;
		}
		else
		{ PushObj(x);
		  obj_prev = PREV_OBJ;
		}
	      }
	      else /* success */
	      { obj_prev = PREV_OP;
	        Shift(x, NO_PREC, 0, FALSE, has_rpar(xsym));
	      }
	    }
	  }
	} /* end if has_target */

	if( filter(xsym) != nil )
	{
	  if( type(t) == BEGIN || type(t) == LBR )
	  { OBJECT filter_stub;

	    /* create filter object and copy parameter into temp file */
	    tmp = FilterCreate((BOOLEAN) (type(t) == BEGIN), xsym, &fpos(t));

	    /* push filter object onto stacks and keep going */
	    Shift(t, precedence(t), 0, FALSE, TRUE);
	    ShiftObj(tmp, PREV_OBJ);
	    t = LexGetToken();
	  }
	  else Error(6, 34, "right parameter of %s must be enclosed in braces",
		 FATAL, &fpos(x), SymName(xsym));
	}

	else if( has_body(xsym) )
	{ if( type(t) == BEGIN || type(t) == LBR )
	  { PushScope(xsym, FALSE, TRUE);
	    PushScope(ChildSym(xsym, RPAR), FALSE, FALSE);
	    PushObj( Parse(&t, encl, FALSE, TRUE) );
	    obj_prev = Reduce();
	    PopScope();
	    PopScope();
	    if( t == nil )  t = LexGetToken();
	  }
	  else
	  { Error(6, 35, "body parameter of %s must be enclosed in braces",
	      WARN, &fpos(t), SymName(xsym));
	  }
	}
	break;


      case OPEN:

	x = t;  xsym = nil;
	Shift(t, precedence(t), RIGHT_ASSOC, TRUE, TRUE);
	if( type(ObjTop) == CLOSURE )  xsym = actual(ObjTop);
	else if( type(ObjTop) == CROSS && Down(ObjTop) != ObjTop )
	{ Child(tmp, Down(ObjTop));
	  if( type(tmp) == CLOSURE )  xsym = actual(tmp);
	}
	t = LexGetToken();

	if( xsym == nil )
	  Error(6, 36, "invalid left parameter of %s", WARN, &fpos(x), KW_OPEN);
	else if( type(t) != BEGIN && type(t) != LBR )
	  Error(6, 37, "right parameter of %s must be enclosed in braces",
	    WARN, &fpos(t), KW_OPEN);
	else
	{ PushScope(xsym, FALSE, TRUE);
	  tmp = Parse(&t, encl, FALSE, FALSE);
	  ShiftObj(tmp, PREV_RBR);
	  PopScope();
	  if( t == nil )  t = LexGetToken();
	  obj_prev = Reduce();
	}
	break;


      default:
      
	Error(6, 38, "Parse: %s", INTERN, &fpos(t), Image(type(t)) );
	break;

    } /* end switch */
  } /* end for */

} /* end Parse */
