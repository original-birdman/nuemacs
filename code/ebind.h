/*      ebind.h
 *
 *      Initial default key to function bindings (for use by main.c)
*
 *      Modified by Petri Kutvonen
 */

#ifndef EBIND_H_
#define EBIND_H_

#include "line.h"

/* This is the array of key-mappings.
 * It is allocated dynamically and initialized from init_keytab
 * at start-up.
 */
struct key_tab *keytab;
#define KEYTAB_INCR 32

/* NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE
 * When in the minibuffer some actions are hard-wired (see
 * getstring() in input.c)
 *
 *  META|CONTROL|'I'    rotate the search string when searching
 *  CTLX|CONTROL|'I'    insert the current saecrh string when searching
 *  CTLX|CONTROL|'M'    evaluate the line, submit the result
 *  CONTROL|'M' (== <Return>)  or CTLX|CONTROL|'C'
 *                      submit the command
 * CONTROL|'G'          abort
 */

/* Command table.
 * This table  is *roughly* in ASCII order, left to right across the
 * characters of the command. This explains the funny location of the
 * control-X commands.
 * This is the standard binding and is copied to the dynamically
 * allocated keytab at start-up. It only contains live entries - the
 * allocation mechanism initializes keytab with ENDL_KMAP entries and
 * an ENDS_KMAP entry as the final element.
 */

int key_index_valid = 0;

struct key_tab_init init_keytab[] = {
    {CONTROL|'A',       gotobol         },
    {CONTROL|'B',       backchar        },
    {CONTROL|'C',       insspace        },
    {CONTROL|'D',       forwdel         },
    {CONTROL|'E',       gotoeol         },
    {CONTROL|'F',       forwchar        },
    {CONTROL|'G',       ctrlg           },
    {CONTROL|'H',       backdel         },
    {CONTROL|'I',       insert_tab      },
    {CONTROL|'J',       indent          },
    {CONTROL|'K',       killtext        },
    {CONTROL|'L',       redraw          },
    {CONTROL|'M',       insert_newline  },
    {CONTROL|'N',       forwline        },
    {CONTROL|'O',       openline        },
    {CONTROL|'P',       backline        },
    {CONTROL|'Q',       quote           },
    {CONTROL|'R',       backsearch      },
    {CONTROL|'S',       forwsearch      },
    {CONTROL|'T',       twiddle         },
    {CONTROL|'U',       unarg           },
    {CONTROL|'V',       forwpage        },
    {CONTROL|'W',       killregion      },
    {CONTROL|'X',       cex             },
    {CONTROL|'Y',       yank            },
    {CONTROL|'Z',       backpage        },
    {CONTROL|']',       metafn          },
    {CTLX|CONTROL|'B',  listbuffers     },
    {CTLX|CONTROL|'C',  quit            }, /* Hard quit. */
    {CTLX|CONTROL|'A',  detab           },
    {CTLX|CONTROL|'D',  filesave        }, /* alternative */
    {CTLX|CONTROL|'E',  entab           },
    {CTLX|CONTROL|'F',  filefind        },
    {CTLX|CONTROL|'I',  insfile         },
    {CTLX|CONTROL|'L',  lowerregion     },
    {CTLX|CONTROL|'M',  delmode         },
    {CTLX|CONTROL|'N',  mvdnwind        },
    {CTLX|CONTROL|'O',  deblank         },
    {CTLX|CONTROL|'P',  mvupwind        },
    {CTLX|CONTROL|'R',  fileread        },
    {CTLX|CONTROL|'S',  filesave        },
    {CTLX|CONTROL|'T',  trim            },
    {CTLX|CONTROL|'U',  upperregion     },
    {CTLX|CONTROL|'V',  viewfile        },
    {CTLX|CONTROL|'W',  filewrite       },
    {CTLX|CONTROL|'X',  swapmark        },
    {CTLX|CONTROL|'Z',  shrinkwind      },
    {CTLX|'?',          deskey          },
    {CTLX|'!',          spawn           },
    {CTLX|'@',          pipecmd         },
    {CTLX|'#',          filter_buffer   },
    {CTLX|'$',          execprg         },
    {CTLX|'=',          showcpos        },
    {CTLX|'(',          ctlxlp          },
    {CTLX|')',          ctlxrp          },
    {CTLX|'^',          enlargewind     },
    {CTLX|'0',          delwind         },
    {CTLX|'1',          onlywind        },
    {CTLX|'2',          splitwind       },
    {CTLX|'A',          setvar          },
    {CTLX|'B',          usebuffer       },
    {CTLX|'C',          spawncli        },
    {CTLX|'D',          bktoshell       },
    {CTLX|'E',          ctlxe           },
    {CTLX|'F',          setfillcol      },
    {CTLX|'K',          killbuffer      },
    {CTLX|'M',          setemode        },
    {CTLX|'N',          filename        },
    {CTLX|'O',          nextwind        },
    {CTLX|'P',          prevwind        },
    {CTLX|'Q',          quote           }, /* alternative */
    {CTLX|'R',          risearch        },
    {CTLX|'S',          fisearch        },
    {CTLX|'W',          resize          },
    {CTLX|'X',          nextbuffer      },
    {CTLX|'Z',          enlargewind     },
    {META|CONTROL|'C',  wordcount       },
    {META|CONTROL|'E',  execproc        },
    {META|CONTROL|'F',  getfence        },
    {META|CONTROL|'H',  delbword        },
    {META|CONTROL|'K',  unbindkey       },
    {META|CONTROL|'L',  reposition      },
    {META|CONTROL|'M',  delgmode        },
    {META|CONTROL|'N',  namebuffer      },
    {META|CONTROL|'R',  qreplace        },
    {META|CONTROL|'V',  scrnextdw       },
    {META|CONTROL|'W',  killpara        },
    {META|CONTROL|'Z',  scrnextup       },
    {META|' ',          setmark         },
    {META|'?',          help            },
    {META|'!',          reposition      },
    {META|'.',          setmark         },
    {META|'>',          gotoeob         },
    {META|'<',          gotobob         },
    {META|'~',          unmark          },
    {META|'A',          apro            },
    {META|'B',          backword        },
    {META|'C',          capword         },
    {META|'D',          delfword        },
    {META|'E',      set_encryption_key  },
    {META|'F',          forwword        },
    {META|'G',          gotoline        },
    {META|'J',          justpara        },
    {META|'K',          bindtokey       },
    {META|'L',          lowerword       },
    {META|'M',          setgmode        },
    {META|'N',          gotoeop         },
    {META|'P',          gotobop         },
    {META|'Q',          fillpara        },
    {META|'R',          sreplace        },
    {META|'S',          forwsearch      }, /* alternative P.K. */
    {META|'U',          upperword       },
    {META|'V',          backpage        },
    {META|'W',          copyregion      },
    {META|'Y',          yank_replace   },
    {META|'X',          namedcmd        },
    {META|'Z',          quickexit       },
    {META|0x7F,         delbword        },

    {0x7F,              backdel         },  /* Delete Key */

/* Special internal bindings */
    {SPEC|META|'W',     wrapword        }, /* called on word wrap */
    {SPEC|META|'C',     nullproc        }, /*  every command input */
    {SPEC|META|'R',     nullproc        }, /*  on file read */
    {SPEC|META|'X',     nullproc        }, /*  on window change P.K. */
};

#endif  /* EBIND_H_ */
