/* Copyright (c) 1986 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "$SunId$ LBL";
#endif

/*
 *  oconv.c - main program for object to octree conversion.
 *
 *     7/29/85
 */

#include  "standard.h"

#include  "octree.h"

#include  "object.h"

#include  "otypes.h"

#define  OMARGIN	(10*FTINY)	/* margin around global cube */

#define  MAXOBJFIL	63		/* maximum number of scene files */

char  *progname;			/* argv[0] */

char  *libpath;				/* library search path */

int  nowarn = 0;			/* supress warnings? */

int  objlim = 5;			/* # of objects before split */

int  resolu = 1024;			/* octree resolution limit */

CUBE  thescene = {EMPTY, {0.0, 0.0, 0.0}, 0.0};		/* our scene */

char  *ofname[MAXOBJFIL+1];		/* object file names */
int  nfiles = 0;			/* number of object files */

double  mincusize;			/* minimum cube size from resolu */


main(argc, argv)		/* convert object files to an octree */
int  argc;
char  **argv;
{
	char  *getenv();
	double  atof();
	FVECT  bbmin, bbmax;
	char  *infile = NULL;
	int  outflags = IO_ALL;
	OBJECT  startobj;
	int  i;

	progname = argv[0];

	if ((libpath = getenv("RAYPATH")) == NULL)
		libpath = ":/usr/local/lib/ray";

	for (i = 1; i < argc && argv[i][0] == '-'; i++)
		switch (argv[i][1]) {
		case 'i':				/* input octree */
			infile = argv[++i];
			break;
		case 'b':				/* bounding cube */
			thescene.cuorg[0] = atof(argv[++i]) - OMARGIN;
			thescene.cuorg[1] = atof(argv[++i]) - OMARGIN;
			thescene.cuorg[2] = atof(argv[++i]) - OMARGIN;
			thescene.cusize = atof(argv[++i]) + 2*OMARGIN;
			break;
		case 'n':				/* set limit */
			objlim = atoi(argv[++i]);
			break;
		case 'r':				/* resolution limit */
			resolu = atoi(argv[++i]);
			break;
		case 'f':				/* freeze octree */
			outflags &= ~IO_FILES;
			break;
		case 'w':				/* supress warnings */
			nowarn = 1;
			break;
		default:
			sprintf(errmsg, "unknown option: '%s'", argv[i]);
			error(USER, errmsg);
			break;
		}
	
	if (infile != NULL) {		/* get old octree & objects */
		if (thescene.cusize > FTINY)
			error(USER, "only one of '-b' or '-i'");
		nfiles = readoct(infile, IO_ALL, &thescene, ofname);
		if (nfiles == 0 && outflags & IO_FILES) {
			error(WARNING, "frozen octree");
			outflags &= ~IO_FILES;
		}
	}

	printargs(argc, argv, stdout);	/* info. header */
	printf("\n");

	startobj = nobjects;		/* previous objects already converted */
	
	for ( ; i < argc; i++) {		/* read new files */
		if (nfiles >= MAXOBJFIL)
			error(INTERNAL, "too many scene files");
		readobj(ofname[nfiles++] = argv[i]);
	}
	ofname[nfiles] = NULL;
						/* find bounding box */
	bbmin[0] = bbmin[1] = bbmin[2] = FHUGE;
	bbmax[0] = bbmax[1] = bbmax[2] = -FHUGE;
	for (i = startobj; i < nobjects; i++)
		add2bbox(objptr(i), bbmin, bbmax);
						/* set/check cube */
	if (thescene.cusize == 0.0) {
		if (bbmin[0] <= bbmax[0]) {
			for (i = 0; i < 3; i++) {
				bbmin[i] -= OMARGIN;
				bbmax[i] += OMARGIN;
			}
			for (i = 0; i < 3; i++)
				if (bbmax[i] - bbmin[i] > thescene.cusize)
					thescene.cusize = bbmax[i] - bbmin[i];
			for (i = 0; i < 3; i++)
				thescene.cuorg[i] =
					(bbmax[i]+bbmin[i]-thescene.cusize)*.5;
		}
	} else {
		for (i = 0; i < 3; i++)
			if (bbmin[i] < thescene.cuorg[i] ||
				bbmax[i] > thescene.cuorg[i] + thescene.cusize)
				error(USER, "boundary does not encompass scene");
	}

	mincusize = thescene.cusize / resolu - FTINY;
		
	for (i = startobj; i < nobjects; i++)		/* add new objects */
		addobject(&thescene, i);
	
	thescene.cutree = combine(thescene.cutree);	/* optimize */

	writeoct(outflags, &thescene, ofname);	/* write structures to stdout */

	quit(0);
}


quit(code)				/* exit program */
int  code;
{
	exit(code);
}


cputs()					/* interactive error */
{
	/* referenced, but not used */
}


wputs(s)				/* warning message */
char  *s;
{
	if (!nowarn)
		eputs(s);
}


eputs(s)				/* put string to stderr */
register char  *s;
{
	static int  inline = 0;

	if (!inline++) {
		fputs(progname, stderr);
		fputs(": ", stderr);
	}
	fputs(s, stderr);
	if (*s && s[strlen(s)-1] == '\n')
		inline = 0;
}


addobject(cu, obj)			/* add an object to a cube */
register CUBE  *cu;
OBJECT  obj;
{
	CUBE  cukid;
	OCTREE  ot;
	OBJECT  oset[MAXSET+1];
	int  in;
	register int  i, j;

	in = (*ofun[objptr(obj)->otype].funp)(objptr(obj), cu);

	if (!in)
		return;				/* no intersection */
	
	if (istree(cu->cutree)) {
						/* do children */
		cukid.cusize = cu->cusize * 0.5;
		for (i = 0; i < 8; i++) {
			cukid.cutree = octkid(cu->cutree, i);
			for (j = 0; j < 3; j++) {
				cukid.cuorg[j] = cu->cuorg[j];
				if ((1<<j) & i)
					cukid.cuorg[j] += cukid.cusize;
			}
			addobject(&cukid, obj);
			octkid(cu->cutree, i) = cukid.cutree;
		}
		
	} else if (isempty(cu->cutree)) {
						/* singular set */
		oset[0] = 1; oset[1] = obj;
		cu->cutree = fullnode(oset);
		
	} else {
						/* add to full node */
		objset(oset, cu->cutree);
		cukid.cusize = cu->cusize * 0.5;
		
		if (in == 2 || oset[0] < objlim || cukid.cusize < mincusize) {
							/* add to set */
			if (oset[0] >= MAXSET) {
				sprintf(errmsg,
					"set overflow in addobject (%s)",
						objptr(obj)->oname);
				error(INTERNAL, errmsg);
			}
			insertelem(oset, obj);
			cu->cutree = fullnode(oset);

		} else {
							/* subdivide cube */
			if ((ot = octalloc()) == EMPTY)
				error(SYSTEM, "out of octree space");
			for (i = 0; i < 8; i++) {
				cukid.cutree = EMPTY;
				for (j = 0; j < 3; j++) {
					cukid.cuorg[j] = cu->cuorg[j];
					if ((1<<j) & i)
						cukid.cuorg[j] += cukid.cusize;
				}
				for (j = 1; j <= oset[0]; j++)
					addobject(&cukid, oset[j]);
				addobject(&cukid, obj);
				octkid(ot, i) = cukid.cutree;
			}
			cu->cutree = ot;
		}
	}
}
