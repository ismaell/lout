/*@z16.c:Size Adjustment:SetNeighbours(), AdjustSize()@***********************/
/*                                                                           */
/*  LOUT: A HIGH-LEVEL LANGUAGE FOR DOCUMENT FORMATTING (VERSION 2.03)       */
/*  COPYRIGHT (C) 1993 Jeffrey H. Kingston                                   */
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
/*  FILE:         z16.c                                                      */
/*  MODULE:       Size Adjustment                                            */
/*  EXTERNS:      SetNeighbours(), AdjustSize()                              */
/*                                                                           */
/*****************************************************************************/
#include "externs"


/*****************************************************************************/
/*                                                                           */
/*  SetNeighbours(link, ratm, pg, pdef, sg, sdef, side)                      */
/*                                                                           */
/*  This is a utility routine used by CatConstrained(), AdjustSize(),        */
/*  BreakTable() and FlushGalley() for calculating size updates in objects.  */
/*  Assuming that link is the link of a component of a VCAT etc., and that   */
/*  ratm is TRUE if there is a marked component to the right of link, set    */
/*                                                                           */
/*     pg    to the gap separating link from the first definite object       */
/*           to the left, or nil if none.  If pg != nil, set pdef to         */
/*           the preceding definite object;  else pdef is undefined.         */
/*                                                                           */
/*     sg    to the gap separating link from the first definite object       */
/*           to the right, or nil if none.  if sg != nil, set sdef to        */
/*           the succeeding definite object;  else sdef is undefined.        */
/*                                                                           */
/*     side  to the side of the mark link is on; either BACK, ON or FWD.     */
/*                                                                           */
/*****************************************************************************/

SetNeighbours(link, ratm, pg, pdef, sg, sdef, side)
OBJECT link;  BOOLEAN ratm;
OBJECT *pg, *pdef, *sg, *sdef;  int *side;
{ OBJECT plink, slink;

  /* find preceding definite; if it exists, set *pg */
  *pg = nil;
  for( plink = PrevDown(link);  type(plink) == LINK;  plink = PrevDown(plink) )
  { Child(*pdef, plink);
    if( type(*pdef) == SPLIT ? SplitIsDefinite(*pdef) : is_definite(type(*pdef)) )
    { Child(*pg, PrevDown(link));
      while( is_index(type(*pg)) )
      {	link = PrevDown(link);
	Child(*pg, PrevDown(link));
      }
      assert( type(*pg) == GAP_OBJ, "SetNeighbours: type(*pg)!" );
      break;
    }
  }

  /* find succeeding definite; if it exists, set *sg */
  *sg = nil;
  for( slink = NextDown(link);  type(slink) == LINK;  slink = NextDown(slink) )
  { Child(*sdef, slink);
    if( type(*sdef) == SPLIT ? SplitIsDefinite(*sdef) : is_definite(type(*sdef)) )
    { Child(*sg, PrevDown(slink));
      while( is_index(type(*sg)) )
      {	slink = PrevDown(slink);
	Child(*sg, PrevDown(slink));
      }
      assert( type(*sg) == GAP_OBJ, "SetNeighbours: type(*sg)!" );
      break;
    }
  }

  *side = ratm ? BACK : *pg == nil || mark(gap(*pg)) ? ON : FWD;
  debug4(DSA,DD, "SetNeighbours: ratm == %s, pg %s nil, sg %s nil, side == %s",
	bool(ratm), *pg == nil ? "==" : "!=", *sg == nil ? "==" : "!=", 
	*side == BACK ? "BACK" : *side == ON ? "ON" : "FWD");
} /* end SetNeighbours */


/*@@**************************************************************************/
/*                                                                           */
/*  static CatAdjustSize(x, b, f, ratm, y, dim)                              */
/*                                                                           */
/*  Adjust the size of x to be *b, *f.  Object x is known to lie in add-set  */
/*  y;  ratm is TRUE iff there is a mark to the right of x.  Return the      */
/*  new size of y in *b, *f.                                                 */
/*                                                                           */
/*****************************************************************************/

static CatAdjustSize(x, b, f, ratm, y, dim)
OBJECT x;  LENGTH *b, *f;  BOOLEAN ratm;  OBJECT y;  int dim;
{ OBJECT link;
  OBJECT pg, prec_def, sg, sd;
  LENGTH beffect, feffect, seffect;  int side;
  int bb, ff;

  debug6(DSA, D, "CatAdjustSize(%s x, %s, %s, %s, %s y, %s)", Image(type(x)),
    EchoLength(*b), EchoLength(*f), bool(ratm), Image(type(y)), dimen(dim));
  debug2(DSA,DD, "x(%s,%s) =", EchoLength(back(x,dim)), EchoLength(fwd(x,dim)));
  ifdebug(DSA, DD, EchoObject(stderr, x));
  debug2(DSA,DD, "y(%s,%s) =", EchoLength(back(y,dim)), EchoLength(fwd(y,dim)));
  ifdebug(DSA, DD, EchoObject(stderr, y));

  /* DO_ADJUST ACAT is a special case because adjustment affects its size */
  if( dim==COL && type(y)==ACAT && display_style(save_style(y)) == DO_ADJUST )
  { back(x, dim) = *b;  fwd(x, dim) = *f;
    *b = back(y, dim);  *f = fwd(y, dim);
    debug2(DSA, D, "CatAdjustSize ACAT %s,%s", EchoLength(*b), EchoLength(*f));
    return;
  }

  link = UpDim(x, dim);
  SetNeighbours(link, ratm, &pg, &prec_def, &sg, &sd, &side);
  { ifdebug(DSA, D,
    if( pg != nil && mode(gap(pg)) == NO_MODE )
    { debug1(DSA, D, "NO_MODE gap pg, is_indefinite(x) == %s, y =",
	bool(is_indefinite(type(x))) );
      ifdebug(DSA, D, EchoObject(stderr, y));
    }
    if( sg != nil && mode(gap(sg)) == NO_MODE )
    { debug1(DSA, D, "NO_MODE gap sg, is_indefinite(x) == %s, y =",
	bool(is_indefinite(type(x))) );
      ifdebug(DSA, D, EchoObject(stderr, y));
    }
  ); }
  if( is_indefinite(type(x)) )
  {
    beffect = pg == nil ? *b :
      MinGap(fwd(prec_def, dim), *b, *f, &gap(pg));

    feffect = sg == nil ? *f :
      MinGap(*f, back(sd, dim), fwd(sd, dim), &gap(sg));

    seffect = pg == nil ? sg == nil ? 0 : back(sd, dim) :
      sg == nil ? fwd(prec_def, dim) :
      MinGap(fwd(prec_def, dim), back(sd,dim), fwd(sd,dim), &gap(sg));
  }
  else /* !is_indefinite(type(x)) */
  {
    beffect = pg == nil ?  *b - back(x, dim) :
      MinGap(fwd(prec_def, dim), *b,           *f,          &gap(pg)) -
      MinGap(fwd(prec_def, dim), back(x, dim), fwd(x, dim), &gap(pg));

    feffect = sg == nil ? *f - fwd(x, dim) :
      MinGap(*f,          back(sd, dim), fwd(sd, dim), &gap(sg)) -
      MinGap(fwd(x, dim), back(sd, dim), fwd(sd, dim), &gap(sg));
	
    seffect = 0;
  }

  back(x, dim) = *b;  fwd(x, dim) = *f;
  switch( side )
  {
    case BACK:	bb = back(y, dim) + beffect + feffect - seffect;
		ff = fwd(y, dim);
		break;

    case ON:	bb = back(y, dim) + beffect - seffect;
		ff = fwd(y, dim) + feffect;
		break;

    case FWD:	bb = back(y, dim);
		ff = fwd(y, dim) + beffect + feffect - seffect;
		break;
  }
  if( bb > MAX_LEN || ff > MAX_LEN )
  { debug2(DSA, D, "bb = %s, ff = %s, y =", EchoLength(bb), EchoLength(ff));
    ifdebug(DSA, D, EchoObject(stderr, y));
    debug0(DSA, D, "x was");
    ifdebug(DSA, D, EchoObject(stderr, x));
    Error(FATAL,&fpos(y),"maximum size (%s) exceeded", EchoLength(MAX_LEN));
  }
  *b = bb;  *f = ff;
  debug2(DSA, D, "CatAdjustSize returning %s,%s", EchoLength(*b), EchoLength(*f));
} /* end CatAdjustSize */


/*@@**************************************************************************/
/*                                                                           */
/*  AdjustSize(x, b, f, dim)                                                 */
/*                                                                           */
/*  Adjust the size of object x, in dimension dim, to be b, f.  If x is a    */
/*  CLOSURE, the adjustment is from a CLOSURE to a definite object of size   */
/*  b, f.                                                                    */
/*                                                                           */
/*****************************************************************************/

AdjustSize(x, b, f, dim)
OBJECT x;  LENGTH b, f;  int dim;
{ OBJECT y, link, tlink, lp, rp, z, index;
  BOOLEAN ratm;  LENGTH tb, tf, cby, cfy, rby, rfy;

  debug6(DSA, D, "[ AdjustSize( %s(%s,%s), %s, %s, %s ), x =",
	Image(type(x)), EchoLength(back(x, dim)), EchoLength(fwd(x, dim)),
	EchoLength(b), EchoLength(f), dimen(dim));
  ifdebug(DSA, DD, EchoObject(stderr, x) );

  while( b != back(x, dim) || f != fwd(x, dim) || is_indefinite(type(x)) )
  { assert( Up(x) != x, "AdjustSize: Up(x) == x!" );
    assert( b >= 0, "AdjustSize: b < 0!" );
    assert( f >= 0, "AdjustSize: f < 0!" );

    /* these cases are unique because they have multiple parents */
    if( type(x) == COL_THR || type(x) == ROW_THR )
    { assert( (type(x)==COL_THR) == (dim==COL), "AdjustSize: COL_THR!" );
      back(x, dim) = b;  fwd(x, dim) = f;
      for( link = Up(x);  link != x;  link = NextUp(link) )
      { Parent(y, link);
	assert( type(y) == SPLIT, "AdjustSize: type(y) != SPLIT!") ;
	AdjustSize(y, b, f, dim);
      }
      debug0(DSA, D, "[ AdjustSize (thread case) returning.");
      return;
    }

    link = UpDim(x, dim);  ratm = FALSE;
    for( tlink=NextDown(link);  type(tlink) == LINK;  tlink=NextDown(tlink) )
    { Child(y, tlink);
      if( type(y) == GAP_OBJ && mark(gap(y)) )  ratm = TRUE;
    }
    y = tlink;

    debug5(DSA, DD, "  b = %s, f = %s, y = %s(%s,%s), x =",
	EchoLength(b), EchoLength(f), Image(type(y)),
	EchoLength(back(y, dim)), EchoLength(fwd(y, dim)));
    ifdebug(DSA, DD, EchoObject(stderr, x) );

    switch( type(y) )
    {

      case HEAD:
      
	if( dim == ROW )
	{ back(x, dim) = b, fwd(x, dim) = f;
	  debug0(DSA, D, "] AdjustSize ROW returning at HEAD");
	  return;
	}
	else
	{
	  /* let lp and rp be the gaps delimiting the          */
	  /* components joined to x                            */
	  for( lp = PrevDown(link);  lp != y;  lp = PrevDown(lp) )
	  { Child(z, lp);
	    if( type(z) == GAP_OBJ && !join(gap(z)) )  break;
	  }
	  for( rp = NextDown(link);  rp != y;  rp = NextDown(rp) )
	  { Child(z, rp);
	    if( type(z) == GAP_OBJ && !join(gap(z)) )  break;
	  }

	  back(x, dim) = b;  fwd(x, dim) = f;
	  if( lp == y && rp == y && !seen_nojoin(y) )
	  {	
	    /* if whole object is joined, do this */
	    b = max(b, back(y, dim));
	    f = max(f, fwd(y, dim));
	  }
	  else
	  {
	    /* if // or || is present, do this */
	    tb = tf = 0;
	    for( link = NextDown(lp);  link != rp;  link = NextDown(link) )
	    { Child(z, link);
	      if( type(z) == GAP_OBJ || is_index(type(z)) )  continue;
	      tb = max(tb, back(z, dim));
	      tf = max(tf, fwd(z, dim));
	    }
	    b = 0;  f = max(tb + tf, fwd(y, dim));
	  }
	  if( back(y, dim) == b && fwd(y, dim) == f )
	  {
	    debug0(DSA, D, "] AdjustSize (COL) returning at HEAD (no wider)");
	    return;
	  }
	  debug3(DGF, D, "AdjustSize widening %s to %s,%s",
		   SymName(actual(y)), EchoLength(b), EchoLength(f));
	  back(y, dim) = b;  fwd(y, dim) = f;
	  if( Up(y) == y )
	  {
	    debug0(DSA, D, "] AdjustSize ret. at HEAD (no parent)" );
	    return;
	  }
	  Parent(index, Up(y));
	  if( type(index) != RECEIVING )
	  {
	    debug1(DSA,D, "] AdjustSize ret. at HEAD (%s)", Image(type(index)));
	    return;
	  }
	  assert( actual(index) != nil, "AdjustSize: actual(index) == nil!" );
	  assert( type(actual(index)) == CLOSURE, "AdjustSize: index non-C!" );
	  if( actual(actual(index)) != GalleySym )
	  {
	    debug0(DSA, D, "] AdjustSize ret. at HEAD (not @Galley, so root)" );
	    return;
	  }
	  y = actual(index);
	}
	break;


      case SPLIT:
      case HCONTRACT:
      case VCONTRACT:
      case HEXPAND:
      case VEXPAND:
      case PADJUST:
      case HADJUST:
      case VADJUST:
      case ONE_COL:
      case ONE_ROW:
      case GRAPHIC:

	back(x, dim) = b;  fwd(x, dim) = f;
	break;


      case HSCALE:
      case VSCALE:

	back(x, dim) = b;  fwd(x, dim) = f;
	if( (dim==COL) == (type(y)==HSCALE) )
	{ debug0(DSA, D, "] AdjustSize returning at HSCALE or VSCALE");
	  return;
	}
	break;


      case SCALE:

	back(x, dim) = b;  fwd(x, dim) = f;
	if( dim == COL )
	{ b *= bc(constraint(y)) / SF;
	  f *= bc(constraint(y)) / SF;
	}
	else
	{ b *= fc(constraint(y)) / SF;
	  f *= fc(constraint(y)) / SF;
	}
	break;


      case ROTATE:
      
	back(x, dim) = b;  fwd(x, dim) = f;
	RotateSize(&cby, &cfy, &rby, &rfy, x, sparec(constraint(y)));
	if( cby != back(y, COL) || cfy != fwd(y, COL) )
	  AdjustSize(y, cby, cfy, COL);
	if( rby != back(y, ROW) || rfy != fwd(y, ROW) )
	  AdjustSize(y, rby, rfy, ROW);
	debug1(DSA, D, "] AdjustSize returning at %s.", Image(type(y)));
	return;


      case WIDE:
      case HIGH:
      
	if( (type(y) == WIDE) == (dim == COL) )
	{ if( !FitsConstraint(b, f, constraint(y)) )
	  { Error(WARN, &fpos(y), "size constraint %s,%s,%s broken by %s,%s",
	      EchoLength(bc(constraint(y))), EchoLength(bfc(constraint(y))),
	      EchoLength(fc(constraint(y))), EchoLength(b), EchoLength(f));
	    SetConstraint(constraint(y), MAX_LEN, b+f, MAX_LEN);
	  }
	  back(x, dim) = b;  fwd(x, dim) = f;
	  EnlargeToConstraint(&b, &f, &constraint(y));
	}
	else
	{ back(x, dim) = b;
	  fwd(x, dim) = f;
	}
	break;


      case COL_THR:
      case ROW_THR:

	assert( (type(y)==COL_THR) == (dim==COL), "AdjustSize: COL_THR!" );
	back(x, dim) = b;  fwd(x, dim) = f;
	b = max(b, back(y, dim));
	f = max(f, fwd(y, dim));
	break;


      case VCAT:
      case HCAT:
      case ACAT:

	if( (type(y) == VCAT) == (dim == ROW) )
	  CatAdjustSize(x, &b, &f, ratm, y, dim);
	else
	{
	  /* let lp and rp be the gaps bracketing the components joined to x */
	  for( lp = PrevDown(link);  lp != y;  lp = PrevDown(lp) )
	  { Child(z, lp);
	    if( type(z) == GAP_OBJ && !join(gap(z)) )  break;
	  }
	  for( rp = NextDown(link);  rp != y;  rp = NextDown(rp) )
	  { Child(z, rp);
	    if( type(z) == GAP_OBJ && !join(gap(z)) )  break;
	  }

	  back(x, dim) = b;  fwd(x, dim) = f;
	  if( lp == y && rp == y )
	  {
	    /* if whole object is joined, do this */
	    b = max(b, back(y, dim));
	    f = max(f, fwd(y, dim));
	  }
	  else
	  { /* if // or || is present, do this */
	    tb = tf = 0;
	    for( link = NextDown(lp); link != rp;  link = NextDown(link) )
	    { Child(z, link);
	      if( type(z) == GAP_OBJ || is_index(type(z)) )  continue;
	      tb = max(tb, back(z, dim));
	      tf = max(tf, fwd(z, dim));
	    }
	    b = 0;  f = max(tb + tf, fwd(y, dim));
	  }
	}
	break;


      case WORD:
      case CLOSURE:
      case NULL_CLOS:
      case CROSS:
      default:
      
	Error(INTERN, &fpos(y), "AdjustSize: %s", Image(type(y)));
	break;

    } /* end switch */
    x = y;
  } /* end while */
  debug0(DSA, D, "] AdjustSize returning.");
} /* end AdjustSize */
