/*
 * sortformat.c - ps output sorting
 *
 * Copyright © 2011-2023 Jim Warner <james.warner@comcast.net
 * Copyright © 2004-2023 Craig Small <csmall@dropbear.xyz
 * Copyright © 2011-2012 Sami Kerola <kerolasa@iki.fi>
 * Copyright © 1998-2004 Albert Cahalan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <sys/types.h>

#include "misc.h"
#include "xalloc.h"

#include "common.h"

static sf_node *sf_list = NULL;         /* deferred sorting and formatting */
static int have_gnu_sort = 0;           /* if true, "O" must be format */
static int already_parsed_sort = 0;     /* redundantly set in & out of fn */
static int already_parsed_format = 0;


/****************  Parse single format specifier *******************/
static format_node *do_one_spec(const char *spec, const char *override){
  const format_struct *fs;
  const macro_struct *ms;

  fs = search_format_array(spec);
  if(fs){
    int w1, w2;
    format_node *thisnode;
    thisnode = xmalloc(sizeof(format_node));
    if(fs->flags & CF_PIDMAX){
      w1 = (int)procps_pid_length();
      w2 = strlen(fs->head);
      if(w2>w1) w1=w2; // FIXME w/ separate header/body column sizing
    }else{
      w1 = fs->width;
    }
    if(override){
      w2 = strlen(override);
      thisnode->width = (w1>w2)?w1:w2;
      thisnode->name = xstrdup(override);
    }else{
      thisnode->width = w1;
      thisnode->name = xstrdup(fs->head);
    }
    thisnode->pr = fs->pr;
    thisnode->vendor = fs->vendor;
    thisnode->flags = fs->flags;
    thisnode->next = NULL;
    return thisnode;
  }

  /* That failed, so try it as a macro. */
  ms = search_macro_array(spec);
  if(ms){
    format_node *list = NULL;
    format_node *newnode;
    const char *walk;
    int dist;
    char buf[16]; /* trust strings will be short (from above, not user) */
    walk = ms->head;
    while(*walk){
      dist = strcspn(walk, ", ");
      strncpy(buf,walk,dist);
      buf[dist] = '\0';
      newnode = do_one_spec(buf,override); /* call self, assume success */
      newnode->next = list;
      list = newnode;
      walk += dist;
      if(*walk) walk++;
    }
    return list;
  }
  return NULL;   /* bad, spec not found */
}


/************ must wrap user format in default *************/
static void O_wrap(sf_node *sfn, int otype){
  format_node *fnode;
  format_node *endp;
  const char *trailer;

  trailer = (otype=='b') ? "END_BSD" : "END_SYS5" ;

  fnode =  do_one_spec("pid",NULL);
  if(!fnode)catastrophic_failure(__FILE__, __LINE__, _("seriously crashing: goodbye cruel world"));
  endp = sfn->f_cooked; while(endp->next) endp = endp->next;  /* find end */
  endp->next = fnode;

  fnode =  do_one_spec(trailer,NULL);
  if(!fnode)catastrophic_failure(__FILE__, __LINE__, _("seriously crashing: goodbye cruel world"));
  endp = fnode; while(endp->next) endp = endp->next;  /* find end */
  endp->next = sfn->f_cooked;
  sfn->f_cooked = fnode;
}

/******************************************************************
 * Used to parse option AIX field descriptors.
 * Put each completed format_node onto the list starting at ->f_cooked
 */
static const char *aix_format_parse(sf_node *sfn){
  char *buf;                   /* temp copy of arg to hack on */
  char *walk;

  /*** sanity check ***/
  walk = sfn->sf;
  /* state machine */ {
  int c = *walk++;
  if(c!='%'&&c!=' ')
                  goto aix_oops;
  initial:
    if(c=='%')    goto get_desc;
    if(!c)        goto looks_ok;
  /* get_text: */
  get_more:
    c = *walk++;
    if(c=='%')    goto get_desc;
    if(c==' ')    goto get_more;
    if(c)         goto get_more;
    goto looks_ok;
  get_desc:
    c = *walk++;
    if(c&&c!=' ') goto initial;
    return _("missing AIX field descriptor");
  aix_oops:
    return _("improper AIX field descriptor");
  looks_ok:
    ;
  }

  /*** sanity check passed ***/
  buf = xstrdup(sfn->sf);
  walk = sfn->sf;

  while(1){
    format_node *fnode;  /* newly allocated */
    format_node *endp;   /* for list manipulation */

    if(*walk == '%'){
      const aix_struct *aix;
      walk++;
      if(*walk == '%')
        return _("missing AIX field descriptor");
      aix = search_aix_array(*walk);
      walk++;
      if(!aix){
        free(buf);
        return _("unknown AIX field descriptor");
      }
      fnode =  do_one_spec(aix->spec, aix->head);
      if(!fnode){
        free(buf);
        return _("AIX field descriptor processing bug");
      }
    } else {
      size_t len;
      len = strcspn(walk, "%");
      /* ladies and gentlemen, please use the following exit only ... */
      if(!len) break;
      memcpy(buf,walk,len);
      buf[len] = '\0';
      walk += len;
      fnode = xmalloc(sizeof(format_node));
      fnode->width = len < INT_MAX ? len : INT_MAX;
      fnode->name = xstrdup(buf);
      fnode->pr = NULL;     /* checked for */
      fnode->vendor = AIX;
      fnode->flags = CF_PRINT_EVERY_TIME;
      fnode->next = NULL;
    }

    endp = fnode; while(endp->next) endp = endp->next;  /* find end */
    endp->next = sfn->f_cooked;
    sfn->f_cooked = fnode;
  }
  free(buf);
  already_parsed_format = 1;
  return NULL;
}

/***************************************************************
 * Used to parse option O lists. Option O is shared between
 * sorting and formatting. Users may expect one or the other.
 * Put each completed format_node onto the list starting at ->f_cooked
 */
static const char *format_parse(sf_node *sfn){
  char *buf;                   /* temp copy of arg to hack on */
  char *sep_loc;               /* separator location: " \t,\n" */
  char *walk;
  const char *err;       /* error code that could or did happen */
  format_node *fnode;
  int items;
  int need_item;
  static char errbuf[80]; /* for variable-text error message */

  /*** prepare to operate ***/
  buf = xstrdup(sfn->sf);

  /*** sanity check and count items ***/
  need_item = 1; /* true */
  items = 0;
  walk = buf;
  do{
    switch(*walk){
    case ' ': case ',': case '\t': case '\n': case '\0':
    /* Linux extension: allow \t and \n as delimiters */
      if(need_item){
        free(buf);
        goto improper;
      }
      need_item=1;
      break;
    default:
      if(need_item) items++;
      need_item=0;
    }
  } while (*++walk);

  if(!items){
    free(buf);
    goto empty;
  }
#ifdef STRICT_LIST
  if(need_item){    /* can't have trailing deliminator */
    free(buf);
    goto improper;
  }
#else
  if(need_item){    /* allow 1 trailing deliminator */
    *--walk='\0';  /* remove the trailing deliminator */
  }
#endif
  /*** actually parse the list ***/
  walk = buf;
  while(items--){
    format_node *endp;
    char *equal_loc;
    char *colon_loc;
    if(!walk) catastrophic_failure(__FILE__, __LINE__, _("please report this bug"));
    sep_loc = strpbrk(walk," ,\t\n");
    /* if items left, then sep_loc is not in header override */
    if(items && sep_loc) *sep_loc = '\0';
    equal_loc = strpbrk(walk,"=");
    if(equal_loc){   /* if header override */
      *equal_loc = '\0';
      equal_loc++;
    }
    colon_loc = strpbrk(walk,":");
    if(colon_loc){   /* if width override */
      *colon_loc = '\0';
      colon_loc++;
      if(strspn(colon_loc,"0123456789") != strlen(colon_loc) || *colon_loc=='0' || !*colon_loc || atoi(colon_loc) <= 0){
        free(buf);
        goto badwidth;
      }
    }
    fnode =  do_one_spec(walk,equal_loc);
    if(!fnode){
      if(!*errbuf){  /* if didn't already create an error string */
        snprintf(
          errbuf,
          sizeof(errbuf),
          _("unknown user-defined format specifier \"%s\""),
          walk
        );
      }
      free(buf);
      goto unknown;
    }
    if(colon_loc){
      if(fnode->next){
        free(buf);
        goto notmacro;
      }
      // FIXME: enforce signal width to 8, 9, or 16 (grep: SIGNAL wide_signals)
      fnode->width = atoi(colon_loc); // already verified to be a number
      if(fnode->width <= 0) catastrophic_failure(__FILE__, __LINE__, _("please report this bug"));
    }
    endp = fnode; while(endp->next) endp = endp->next;  /* find end */
    endp->next = sfn->f_cooked;
    sfn->f_cooked = fnode;
    walk = sep_loc ? sep_loc + 1 : NULL; /* point to next item, if any */
  }
  free(buf);
  already_parsed_format = 1;
  return NULL;

  /* errors may cause a retry looking for AIX format codes */
  if(0) unknown:  err=errbuf;
  if(0) empty:    err=_("empty format list");
  if(0) improper: err=_("improper format list");
  if(0) badwidth: err=_("column widths must be unsigned decimal numbers");
  if(0) notmacro: err=_("can not set width for a macro (multi-column) format specifier");
  /*
   * Use the AIX parser if the field starts with '%' and is not
   * literally "%cpu", "%mem", "%cpu " or "%mem "
   * as these are regular fields
   * "%cpublah" is treated as %c publah and parsed as AIX format
   * "%cpu blah" is an error
   * %m is not a valid AIX format anyway
   */
  if (strchr(sfn->sf,'%')
    && strcmp("%cpu", sfn->sf) != 0
    && strncmp("%cpu ", sfn->sf,5) != 0
    && strncmp("%cpu,", sfn->sf,5) != 0
    && strncmp("%m", sfn->sf, 2) != 0)
      err = aix_format_parse(sfn);
  return err;
}

/****************  Parse single sort specifier *******************/
static sort_node *do_one_sort_spec(const char *spec){
  const format_struct *fs;
  enum pids_sort_order reverse = PIDS_SORT_ASCEND;
  if(*spec == '-'){
    reverse = PIDS_SORT_DESCEND;
    spec++;
  } else if(*spec == '+'){
    spec++;
  }
  fs = search_format_array(spec);
  if(fs){
    sort_node *thisnode;
    thisnode = xmalloc(sizeof(sort_node));
    thisnode->sr = fs->sr;
    // next is a special pointer, called to help with rel enums
    thisnode->xe = (int(*)(char*,proc_t*))fs->pr;
    thisnode->reverse = reverse;
    thisnode->next = NULL;
    return thisnode;
  }
  return NULL;   /* bad, spec not found */
}


/**************************************************************
 * Used to parse long sorting options.
 * Put each completed sort_node onto the list starting at ->s_cooked
 */
static const char *long_sort_parse(sf_node *sfn){
  char *buf;                   /* temp copy of arg to hack on */
  char *sep_loc;               /* separator location: " \t,\n" */
  char *walk;
  sort_node *snode;
  int items;
  int need_item;

  /*** prepare to operate ***/
  buf = xstrdup(sfn->sf);

  /*** sanity check and count items ***/
  need_item = 1; /* true */
  items = 0;
  walk = buf;
  do{
    switch(*walk){
    case ' ': case ',': case '\t': case '\n': case '\0':
      if(need_item){
        free(buf);
        return _("improper sort list");
      }
      need_item=1;
      break;
    default:
      if(need_item) items++;
      need_item=0;
    }
  } while (*++walk);
  if(!items){
    free(buf);
    return _("empty sort list");
  }
#ifdef STRICT_LIST
  if(need_item){    /* can't have trailing deliminator */
    free(buf);
    return _("improper sort list");
  }
#else
  if(need_item){    /* allow 1 trailing deliminator */
    *--walk='\0';  /* remove the trailing deliminator */
  }
#endif
  /*** actually parse the list ***/
  walk = buf;
  while(items--){
    sort_node *endp;
    sep_loc = strpbrk(walk," ,\t\n");
    if(sep_loc) *sep_loc = '\0';
    snode = do_one_sort_spec(walk);
    if(!snode){
      free(buf);
      return _("unknown sort specifier");
    }
    endp = snode; while(endp->next) endp = endp->next;  /* find end */
    endp->next = sfn->s_cooked;
    sfn->s_cooked = snode;
    walk = sep_loc + 1; /* point to next item, if any */
  }
  free(buf);
  already_parsed_sort = 1;
  return NULL;
}






/************ pre-parse short sorting option *************/
/* Errors _must_ be detected so that the "O" option can try to
 * reparse as formatting codes.
 */
static const char *verify_short_sort(const char *arg){
  const char all[] = "CGJKMNPRSTUcfgjkmnoprstuvy+-";
  char checkoff[256];
  int i;
  const char *walk;
  int tmp;
  if(strspn(arg,all) != strlen(arg)) return _("bad sorting code");
  for(i=256; i--;) checkoff[i] = 0;
  walk = arg;
  for(;;){
    tmp = *walk;
    if(tmp < 0 || (size_t)tmp >= sizeof(checkoff)) return _("bad sorting code");
    switch(tmp){
    case '\0':
      return NULL;   /* looks good */
    case '+':
    case '-':
      tmp = *(walk+1);
      if(!tmp || tmp=='+' || tmp=='-') return _("bad sorting code");
      break;
    case 'P':
      if(forest_type) return _("PPID sort and forest output conflict");
      /* fall through */
    default:
      if(checkoff[tmp]) return _("bad sorting code");   /* repeated */
      /* ought to check against already accepted sort options */
      checkoff[tmp] = 1;
      break;
    }
    walk++;
  }
}



/************ parse short sorting option *************/
static const char *short_sort_parse(sf_node *sfn){
  enum pids_sort_order direction = PIDS_SORT_ASCEND;
  const char *walk;
  int tmp;
  sort_node *snode;
  sort_node *endp;
  const struct shortsort_struct *ss;
  walk = sfn->sf;
  for(;;){
    tmp = *walk;
    switch(tmp){
    case '\0':
      already_parsed_sort = 1;
      return NULL;
    case '+':
      direction = PIDS_SORT_ASCEND;
      break;
    case '-':
      direction = PIDS_SORT_DESCEND;
      break;
    default:
      ss = search_shortsort_array(tmp);
      if(!ss) return _("unknown sort specifier");
      snode = do_one_sort_spec(ss->spec);
      if(!snode) return _("unknown sort specifier");
      snode->reverse = direction;
      endp = snode; while(endp->next) endp = endp->next;  /* find end */
      endp->next = sfn->s_cooked;
      sfn->s_cooked = snode;
      direction = 0;
      break;
    }
    walk++;
  }
}

/******************* high-level below here *********************/


/*
 * Used to parse option O lists. Option O is shared between
 * sorting and formatting. Users may expect one or the other.
 * Recursion is to preserve original order.
 */
static const char *parse_O_option(sf_node *sfn){
  const char *err;     /* error code that could or did happen */

  if(sfn->next){
    err = parse_O_option(sfn->next);
    if(err) return err;
  }

  switch(sfn->sf_code){
    case SF_B_o: case SF_G_format: case SF_U_o: /*** format ***/
      err = format_parse(sfn);
      if(!err) already_parsed_format = 1;
      break;
    case SF_U_O:                                /*** format ***/
      /* Can have -l -f f u... set already_parsed_format like DEC does */
      if(already_parsed_format) return _("option -O can not follow other format options");
      err = format_parse(sfn);
      if(err) return err;
      already_parsed_format = 1;
      O_wrap(sfn,'u'); /* must wrap user format in default */
      break;
    case SF_B_O:                                /***  both  ***/
      if(have_gnu_sort || already_parsed_sort) err = _("multiple sort options");
      else err = verify_short_sort(sfn->sf);
      if(!err){ /* success as sorting code */
        short_sort_parse(sfn);
        already_parsed_sort = 1;
        return NULL;
      }
      if(already_parsed_format){
        err = _("option O is neither first format nor sort order");
        break;
      }
      if(!format_parse(sfn)){ /* if success as format code */
        already_parsed_format = 1;
        O_wrap(sfn,'b'); /* must wrap user format in default */
        return NULL;
      }
      break;
    case SF_G_sort: case SF_B_m:                 /***  sort  ***/
      if(already_parsed_sort) err = _("multiple sort options");
      else err = long_sort_parse(sfn);
      already_parsed_sort = 1;
      break;
    default:                                    /***  junk  ***/
      catastrophic_failure(__FILE__, __LINE__, _("please report this bug"));
  }
  return err; /* could be NULL */
}


/************ Main parser calls this to save lists for later **********/
/* store data for later and return 1 if arg looks non-standard */
int defer_sf_option(const char *arg, int source){
  sf_node *sfn;
  char buf[16];
  int dist;
  const format_struct *fs;
  int need_item = 1;

  sfn = xmalloc(sizeof(sf_node));
  sfn->sf = xstrdup(arg);
  sfn->sf_code = source;
  sfn->s_cooked = NULL;
  sfn->f_cooked = NULL;
  sfn->next = sf_list;
  sf_list = sfn;

  if(source == SF_G_sort) have_gnu_sort = 1;

  /* Now try to find an excuse to ignore broken Unix98 parsing. */
  if(source != SF_U_o) return 1;    /* Wonderful! Already non-Unix98. */
  do{
    switch(*arg){
    case ' ': case ',': case '\0':  /* no \t\n\r support in Unix98 */
      if(need_item) return 1;       /* something wrong */
      need_item=1;
      break;
    case '=':
      if(need_item) return 1;       /* something wrong */
      return 0;                     /* broken Unix98 parsing is required */
    default:
      if(!need_item) break;
      need_item=0;
      dist = strcspn(arg,", =");
      if(dist>15) return 1;         /* something wrong, sort maybe? */
      strncpy(buf,arg,dist);   /* no '\0' on end */
      buf[dist] = '\0';        /* fix that problem */
      fs = search_format_array(buf);
      if(!fs) return 1;             /* invalid spec, macro or sort maybe? */
      if(fs->vendor) return 1;      /* Wonderful! Legal non-Unix98 spec. */
    }
  } while (*++arg);

  return 0;                         /* boring, Unix98 is no change */
}

/***** Since ps is not long-lived, the memory leak can be ignored. ******/
void reset_sortformat(void){
  sf_list = NULL;          /* deferred sorting and formatting */
  format_list = NULL;      /* digested formatting options */
  sort_list = NULL;        /* digested sorting options (redundant?) */
  have_gnu_sort = 0;
  already_parsed_sort = 0;
  already_parsed_format = 0;
}


/***** Search format_list for findme, then insert putme after findme. ****/
static int fmt_add_after(const char *findme, format_node *putme){
  format_node *walk;
  if(!strcmp(format_list->name, findme)){
    putme->next = format_list->next;
    format_list->next = putme;
    return 1; /* success */
  }
  walk = format_list;
  while(walk->next){
    if(!strcmp(walk->next->name, findme)){
      putme->next = walk->next->next;
      walk->next->next = putme;
      return 1; /* success */
    }
    walk = walk->next;
  }
  return 0; /* fail */
}

/******* Search format_list for findme, then delete it. ********/
static int fmt_delete(const char *findme){
  format_node *walk;
  format_node *old;
  if(!strcmp(format_list->name, findme)){
    old = format_list;
    format_list = format_list->next;
    free(old);
    return 1; /* success */
  }
  walk = format_list;
  while(walk->next){
    if(!strcmp(walk->next->name, findme)){
      old = walk->next;
      walk->next = walk->next->next;
      free(old);
      return 1; /* success */
    }
    walk = walk->next;
  }
  return 0; /* fail */
}


/************ Build a SysV format backwards. ***********/
#define PUSH(foo) (fn=do_one_spec(foo, NULL), fn->next=format_list, format_list=fn)
static const char *generate_sysv_list(void){
  format_node *fn;
  if((format_modifiers & FM_y) && !(format_flags & FF_Ul))
    return _("modifier -y without format -l makes no sense");
  if(prefer_bsd_defaults){
    if(format_flags) PUSH("cmd");
    else PUSH("args");
    PUSH("bsdtime");
    if(!(format_flags & FF_Ul)) PUSH("stat");
  }else{
    if(format_flags & FF_Uf) PUSH("cmd");
    else PUSH("ucmd");
    PUSH("time");
  }
  PUSH("tname");  /* Unix98 says "TTY" here, yet "tty" produces "TT". */
  if(format_flags & FF_Uf) PUSH("stime");
  /* avoid duplicate columns from -FP and -Fly */
  if(format_modifiers & FM_F){
    /* if -FP take the Sun-style column instead (sorry about "sgi_p") */
    if(!(format_modifiers & FM_P)) PUSH("psr");  /* should be ENG */
    /* if -Fly take the ADDR-replacement RSS instead */
    if(!( (format_flags & FF_Ul) && (format_modifiers & FM_y) )) PUSH("rss");
  }
  if(format_flags & FF_Ul){
    PUSH("wchan");
  }
  /* since FM_y adds RSS anyway, don't do this hack when that is true */
  if( (format_flags & FF_Ul) && !(format_modifiers & FM_y) ){
    if(personality & PER_IRIX_l){ /* add "rss" then ':' here */
      PUSH("sgi_rss");
      fn = xmalloc(sizeof(format_node));
      fn->width = 1;
      fn->name = xstrdup(":");
      fn->pr = NULL;     /* checked for */
      fn->vendor = AIX;   /* yes, for SGI weirdness */
      fn->flags = CF_PRINT_EVERY_TIME;
      fn->next = format_list;
      format_list=fn;
    }
  }
  if((format_modifiers & FM_F) || (format_flags & FF_Ul)){
    PUSH("sz");
  }
  if(format_flags & FF_Ul){
    if(format_modifiers & FM_y) PUSH("rss");
    else if(personality & (PER_ZAP_ADDR|PER_IRIX_l)) PUSH("sgi_p");
    else PUSH("addr_1");
  }
  if(format_modifiers & FM_c){
    PUSH("pri"); PUSH("class");
  }else if(format_flags & FF_Ul){
    PUSH("ni");
    if(personality & PER_IRIX_l) PUSH("priority");
    else /* is this good? */ PUSH("opri");
  }

  // FIXME TODO XXX -- this is a serious problem
  // These somehow got flipped around.
  // The bug is in procps-3.1.1, procps-990211, prior too?
  if((thread_flags & TF_U_L) && (format_flags & FF_Uf)) PUSH("nlwp");
  if( (format_flags & (FF_Uf|FF_Ul)) && !(format_modifiers & FM_c) ) PUSH("c");

  if(format_modifiers & FM_P) PUSH("psr");
  if(thread_flags & TF_U_L) PUSH("lwp");
  if(format_modifiers & FM_j){
    PUSH("sid");
    PUSH("pgid");
  }
  if(format_flags & (FF_Uf|FF_Ul)) PUSH("ppid");
  if(thread_flags & TF_U_T) PUSH("spid");
  PUSH("pid");
  if(format_flags & FF_Uf){
    if(personality & PER_SANE_USER) PUSH("user");
    else PUSH("uid_hack");
  }else if(format_flags & FF_Ul){
    PUSH("uid");
  }
  if(format_flags & FF_Ul){
    PUSH("s");
    if(!(format_modifiers & FM_y)) PUSH("f");
  }
  if(format_modifiers & FM_M){
    PUSH("label");  /* Mandatory Access Control */
  }
  return NULL;
}


/**************************************************************************
 * Used to parse option O lists. Option O is shared between
 * sorting and formatting. Users may expect one or the other.
 * The "broken" flag enables a really bad Unix98 misfeature.
 */
const char *process_sf_options(void){
  sf_node *sf_walk;

  if(sf_list){
    const char *err;
    err = parse_O_option(sf_list);
    if(err) return err;
  }

  if(format_list) catastrophic_failure(__FILE__, __LINE__, _("bug: must reset the list first"));

  /* merge formatting info of sf_list into format_list here */
  sf_walk = sf_list;
  while(sf_walk){
    format_node *fmt_walk;
    fmt_walk = sf_walk->f_cooked;
    sf_walk->f_cooked = NULL;
    while(fmt_walk){   /* put any nodes onto format_list in opposite way */
      format_node *travler;
      travler = fmt_walk;
      fmt_walk = fmt_walk->next;
      travler->next = format_list;
      format_list = travler;
    }
    sf_walk = sf_walk->next;
  }

  /* merge sorting info of sf_list into sort_list here */
  sf_walk = sf_list;
  while(sf_walk){
    sort_node *srt_walk;
    srt_walk = sf_walk->s_cooked;
    sf_walk->s_cooked = NULL;
    if (srt_walk) {
      sort_node *travler = srt_walk;
      while (travler->next) travler = travler->next;
      travler->next = sort_list;
      sort_list = srt_walk;
    }
    sf_walk = sf_walk->next;
  }

  // Get somebody to explain how -L/-T is supposed to interact
  // with sorting. Do the threads remain grouped, with sorting
  // by process, or do the threads get sorted by themselves?
  if(sort_list && (thread_flags&TF_no_sort)){
    return _("tell <procps@freelists.org> what you expected");
  }

  // If nothing else, try to use $PS_FORMAT before the default.
  if(!format_flags && !format_modifiers && !format_list){
    char *tmp;
    tmp = getenv("PS_FORMAT");  /* user override kills default */
    if(tmp && *tmp){
      const char *err;
      sf_node sfn;
      if(thread_flags&TF_must_use) return _("tell <procps@freelists.org> what you want (-L/-T, -m/m/H, and $PS_FORMAT)");
      sfn.sf = tmp;
      sfn.f_cooked = NULL;
      err = format_parse(&sfn);
      if(!err){
        format_node *fmt_walk;
        fmt_walk = sfn.f_cooked;
        while(fmt_walk){   /* put any nodes onto format_list in opposite way */
          format_node *travler;
          travler = fmt_walk;
          fmt_walk = fmt_walk->next;
          travler->next = format_list;
          format_list = travler;
        }
        return NULL;
      }
      // FIXME: prove that this won't be hit on valid bogus-BSD options
      fprintf(stderr, _("warning: $PS_FORMAT ignored. (%s)\n"), err);
    }
  }

  if(format_list){
    if(format_flags) return _("conflicting format options");
    if(format_modifiers) return _("can not use output modifiers with user-defined output");
    if(thread_flags&TF_must_use) return _("-L/-T with H/m/-m and -o/-O/o/O is nonsense");
    return NULL;
  }

  do{
    const char *spec;
    switch(format_flags){

    default:             return _("conflicting format options");

    /* These can be NULL, which enables SysV list generation code. */
    case 0:              spec=NULL;           break;
    case FF_Uf | FF_Ul:  spec=sysv_fl_format; break;
    case FF_Uf:          spec=sysv_f_format;  break;
    case FF_Ul:          spec=sysv_l_format;  break;

    /* These are NOT REACHED for normal -j processing. */
    case FF_Uj:          spec=sysv_j_format;  break; /* Debian & Digital */
    case FF_Uj | FF_Ul:  spec="RD_lj";        break; /* Debian */
    case FF_Uj | FF_Uf:  spec="RD_fj";        break; /* Debian */

    /* These are true BSD options. */
    case FF_Bj:          spec=bsd_j_format;   break;
    case FF_Bl:          spec=bsd_l_format;   break;
    case FF_Bs:          spec=bsd_s_format;   break;
    case FF_Bu:          spec=bsd_u_format;   break;
    case FF_Bv:          spec=bsd_v_format;   break;

    /* These are old Linux options. Option m is overloaded. */
    case FF_LX:          spec="OL_X";         break;
    case FF_Lm:          spec="OL_m";         break;

    /* This is the sole FLASK security option. */
    case FF_Fc:          spec="FLASK_context"; break;

    }  /* end switch(format_flags) */

    // not just for case 0, since sysv_l_format and such may be NULL
    if(!spec) return generate_sysv_list();

    do{
      format_node *fmt_walk;
      fmt_walk = do_one_spec(spec, NULL); /* use override "" for no headers */
      while(fmt_walk){   /* put any nodes onto format_list in opposite way */
        format_node *travler;
        travler = fmt_walk;
        fmt_walk = fmt_walk->next;
        travler->next = format_list;
        format_list = travler;
      }
    }while(0);
  }while(0);

  do{
    format_node *fn;
    if(format_modifiers & FM_j){
      fn = do_one_spec("pgid", NULL);
      if(!fmt_add_after("PPID", fn)) if(!fmt_add_after("PID", fn))
        catastrophic_failure(__FILE__, __LINE__, _("internal error: no PID or PPID for -j option"));
      fn = do_one_spec("sid", NULL);
      if(!fmt_add_after("PGID", fn)) return _("lost my PGID");
    }
    if(format_modifiers & FM_y){
      /* TODO: check for failure to do something, and complain if so */
      fmt_delete("F");
      fn = do_one_spec("rss", NULL);
      if(fmt_add_after("ADDR", fn)) fmt_delete("ADDR");
    }
    if(format_modifiers & FM_c){
      fmt_delete("%CPU"); fmt_delete("CPU"); fmt_delete("CP"); fmt_delete("C");
      fmt_delete("NI");
      fn = do_one_spec("class", NULL);
      if(!fmt_add_after("PRI", fn))
        catastrophic_failure(__FILE__, __LINE__, _("internal error: no PRI for -c option"));
      fmt_delete("PRI"); /* we want a different one */
      fn = do_one_spec("pri", NULL);
      if(!fmt_add_after("CLS", fn)) return _("lost my CLS");
    }
    if(thread_flags & TF_U_T){
      fn = do_one_spec("spid", NULL);
      if(!fmt_add_after("PID", fn) && (thread_flags&TF_must_use))
        return _("-T with H/-m/m but no PID for SPID to follow");
    }
    if(thread_flags & TF_U_L){
      fn = do_one_spec("lwp", NULL);
      if(fmt_add_after("SID",  fn)) goto did_lwp;
      if(fmt_add_after("SESS", fn)) goto did_lwp;
      if(fmt_add_after("PGID", fn)) goto did_lwp;
      if(fmt_add_after("PGRP", fn)) goto did_lwp;
      if(fmt_add_after("PPID", fn)) goto did_lwp;
      if(fmt_add_after("PID",  fn)) goto did_lwp;
      if(thread_flags&TF_must_use)
        return _("-L with H/-m/m but no PID/PGID/SID/SESS for NLWP to follow");
did_lwp:
      fn = do_one_spec("nlwp", NULL);
      fmt_add_after("%CPU",  fn);
    }
    if(format_modifiers & FM_M){    // Mandatory Access Control, IRIX style
      fn = do_one_spec("label", NULL);
      fn->next=format_list;
      format_list=fn;
    }
    /* Do personality-specific translations not covered by format_flags.
     * Generally, these only get hit when personality overrides unix output.
     * That (mostly?) means the Digital and Debian personalities.
     */
    if((personality & PER_ZAP_ADDR) && (format_flags & FF_Ul)){
      fn = do_one_spec("sgi_p", NULL);
      if(fmt_add_after("ADDR", fn)) fmt_delete("ADDR");
    }
    if((personality & PER_SANE_USER) && (format_flags & FF_Uf)){
      fn = do_one_spec("user", NULL);
      if(fmt_add_after("UID", fn)) fmt_delete("UID");
    }
  }while(0);

  return NULL;
}

