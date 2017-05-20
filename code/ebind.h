/*      ebind.h
 *
 *      Initial default key to function bindings
 *
 *      Modified by Petri Kutvonen
 */

#ifndef EBIND_H_
#define EBIND_H_

#include "line.h"

/*
 * Command table.
 * This table  is *roughly* in ASCII order, left to right across the
 * characters of the command. This explains the funny location of the
 * control-X commands.
 * This is the standard binding and is copied to the dynamically
 * allocated keytab at start-up. It only contians live entries - the
 * allocation mechanism initializes keytab with ENDL_KMAP entries and
 * an ENDS_KMAP entry as the final element.
 */
#define KEYTAB_INCR 32

struct key_tab *keytab;
struct key_tab init_keytab[] = {
        {FUNC_KMAP, CONTROL | 'A', {gotobol}            },
        {FUNC_KMAP, CONTROL | 'B', {back_grapheme}      },
        {FUNC_KMAP, CONTROL | 'C', {insspace}           },
        {FUNC_KMAP, CONTROL | 'D', {forwdel}            },
        {FUNC_KMAP, CONTROL | 'E', {gotoeol}            },
        {FUNC_KMAP, CONTROL | 'F', {forw_grapheme}      },
        {FUNC_KMAP, CONTROL | 'G', {ctrlg}              },
        {FUNC_KMAP, CONTROL | 'H', {backdel}            },
        {FUNC_KMAP, CONTROL | 'I', {insert_tab}         },
        {FUNC_KMAP, CONTROL | 'J', {indent}             },
        {FUNC_KMAP, CONTROL | 'K', {killtext}           },
        {FUNC_KMAP, CONTROL | 'L', {redraw}             },
        {FUNC_KMAP, CONTROL | 'M', {insert_newline}     },
        {FUNC_KMAP, CONTROL | 'N', {forwline}           },
        {FUNC_KMAP, CONTROL | 'O', {openline}           },
        {FUNC_KMAP, CONTROL | 'P', {backline}           },
        {FUNC_KMAP, CONTROL | 'Q', {quote}              },
        {FUNC_KMAP, CONTROL | 'R', {backsearch}         },
        {FUNC_KMAP, CONTROL | 'S', {forwsearch}         },
        {FUNC_KMAP, CONTROL | 'T', {twiddle}            },
        {FUNC_KMAP, CONTROL | 'U', {unarg}              },
        {FUNC_KMAP, CONTROL | 'V', {forwpage}           },
        {FUNC_KMAP, CONTROL | 'W', {killregion}         },
        {FUNC_KMAP, CONTROL | 'X', {cex}                },
        {FUNC_KMAP, CONTROL | 'Y', {yank}               },
        {FUNC_KMAP, CONTROL | 'Z', {backpage}           },
        {FUNC_KMAP, CONTROL | ']', {metafn}             },
        {FUNC_KMAP, CTLX | CONTROL | 'B', {listbuffers} },
        {FUNC_KMAP, CTLX | CONTROL | 'C', {quit}        }, /* Hard quit. */
#if     AEDIT
        {FUNC_KMAP, CTLX | CONTROL | 'A', {detab}       },
#endif
        {FUNC_KMAP, CTLX | CONTROL | 'D', {filesave}    }, /* alternative */
#if     AEDIT
        {FUNC_KMAP, CTLX | CONTROL | 'E', {entab}       },
#endif
        {FUNC_KMAP, CTLX | CONTROL | 'F', {filefind}    },
        {FUNC_KMAP, CTLX | CONTROL | 'I', {insfile}     },
        {FUNC_KMAP, CTLX | CONTROL | 'L', {lowerregion} },
        {FUNC_KMAP, CTLX | CONTROL | 'M', {delmode}     },
        {FUNC_KMAP, CTLX | CONTROL | 'N', {mvdnwind}    },
        {FUNC_KMAP, CTLX | CONTROL | 'O', {deblank}     },
        {FUNC_KMAP, CTLX | CONTROL | 'P', {mvupwind}    },
        {FUNC_KMAP, CTLX | CONTROL | 'R', {fileread}    },
        {FUNC_KMAP, CTLX | CONTROL | 'S', {filesave}    },
#if     AEDIT
        {FUNC_KMAP, CTLX | CONTROL | 'T', {trim}        },
#endif
        {FUNC_KMAP, CTLX | CONTROL | 'U', {upperregion} },
        {FUNC_KMAP, CTLX | CONTROL | 'V', {viewfile}    },
        {FUNC_KMAP, CTLX | CONTROL | 'W', {filewrite}   },
        {FUNC_KMAP, CTLX | CONTROL | 'X', {swapmark}    },
        {FUNC_KMAP, CTLX | CONTROL | 'Z', {shrinkwind}  },
        {FUNC_KMAP, CTLX | '?', {deskey}                },
        {FUNC_KMAP, CTLX | '!', {spawn}                 },
        {FUNC_KMAP, CTLX | '@', {pipecmd}               },
        {FUNC_KMAP, CTLX | '#', {filter_buffer}         },
        {FUNC_KMAP, CTLX | '$', {execprg}               },
        {FUNC_KMAP, CTLX | '=', {showcpos}              },
        {FUNC_KMAP, CTLX | '(', {ctlxlp}                },
        {FUNC_KMAP, CTLX | ')', {ctlxrp}                },
        {FUNC_KMAP, CTLX | '^', {enlargewind}           },
        {FUNC_KMAP, CTLX | '0', {delwind}               },
        {FUNC_KMAP, CTLX | '1', {onlywind}              },
        {FUNC_KMAP, CTLX | '2', {splitwind}             },
        {FUNC_KMAP, CTLX | 'A', {setvar}                },
        {FUNC_KMAP, CTLX | 'B', {usebuffer}             },
        {FUNC_KMAP, CTLX | 'C', {spawncli}              },
/* Seems to screw-up the tty, so disable */
#if 0
#if     BSD | __hpux | SVR4
        {FUNC_KMAP, CTLX | 'D', {bktoshell}             },
#endif
#endif
        {FUNC_KMAP, CTLX | 'E', {ctlxe}                 },
        {FUNC_KMAP, CTLX | 'F', {setfillcol}            },
        {FUNC_KMAP, CTLX | 'K', {killbuffer}            },
        {FUNC_KMAP, CTLX | 'M', {setemode}              },
        {FUNC_KMAP, CTLX | 'N', {filename}              },
        {FUNC_KMAP, CTLX | 'O', {nextwind}              },
        {FUNC_KMAP, CTLX | 'P', {prevwind}              },
        {FUNC_KMAP, CTLX | 'Q', {quote}                 }, /* alternative */
#if     ISRCH
        {FUNC_KMAP, CTLX | 'R', {risearch}              },
        {FUNC_KMAP, CTLX | 'S', {fisearch}              },
#endif
        {FUNC_KMAP, CTLX | 'W', {resize}                },
        {FUNC_KMAP, CTLX | 'X', {nextbuffer}            },
        {FUNC_KMAP, CTLX | 'Z', {enlargewind}           },
#if     WORDPRO
        {FUNC_KMAP, META | CONTROL | 'C', {wordcount}   },
#endif
        {FUNC_KMAP, META | CONTROL | 'D', {newsize}     },
#if     PROC
        {FUNC_KMAP, META | CONTROL | 'E', {execproc}    },
#endif
#if     CFENCE
        {FUNC_KMAP, META | CONTROL | 'F', {getfence}    },
#endif
        {FUNC_KMAP, META | CONTROL | 'H', {delbword}    },
        {FUNC_KMAP, META | CONTROL | 'K', {unbindkey}   },
        {FUNC_KMAP, META | CONTROL | 'L', {reposition}  },
        {FUNC_KMAP, META | CONTROL | 'M', {delgmode}    },
        {FUNC_KMAP, META | CONTROL | 'N', {namebuffer}  },
        {FUNC_KMAP, META | CONTROL | 'R', {qreplace}    },
        {FUNC_KMAP, META | CONTROL | 'S', {newsize}     },
        {FUNC_KMAP, META | CONTROL | 'T', {newwidth}    },
        {FUNC_KMAP, META | CONTROL | 'V', {scrnextdw}   },
#if     WORDPRO
        {FUNC_KMAP, META | CONTROL | 'W', {killpara}    },
#endif
        {FUNC_KMAP, META | CONTROL | 'Z', {scrnextup}   },
        {FUNC_KMAP, META | ' ', {setmark}               },
        {FUNC_KMAP, META | '?', {help}                  },
        {FUNC_KMAP, META | '!', {reposition}            },
        {FUNC_KMAP, META | '.', {setmark}               },
        {FUNC_KMAP, META | '>', {gotoeob}               },
        {FUNC_KMAP, META | '<', {gotobob}               },
        {FUNC_KMAP, META | '~', {unmark}                },
#if     APROP
        {FUNC_KMAP, META | 'A', {apro}                  },
#endif
        {FUNC_KMAP, META | 'B', {backword}              },
        {FUNC_KMAP, META | 'C', {capword}               },
        {FUNC_KMAP, META | 'D', {delfword}              },
#if     CRYPT
        {FUNC_KMAP, META | 'E', {set_encryption_key}    },
#endif
        {FUNC_KMAP, META | 'F', {forwword}              },
        {FUNC_KMAP, META | 'G', {gotoline}              },
#if     WORDPRO
        {FUNC_KMAP, META | 'J', {justpara}              },
#endif
        {FUNC_KMAP, META | 'K', {bindtokey}             },
        {FUNC_KMAP, META | 'L', {lowerword}             },
        {FUNC_KMAP, META | 'M', {setgmode}              },
#if     WORDPRO
        {FUNC_KMAP, META | 'N', {gotoeop}               },
        {FUNC_KMAP, META | 'P', {gotobop}               },
        {FUNC_KMAP, META | 'Q', {fillpara}              },
#endif
        {FUNC_KMAP, META | 'R', {sreplace}              },
        {FUNC_KMAP, META | 'S', {forwsearch}            }, /* alternative P.K. */
        {FUNC_KMAP, META | 'U', {upperword}             },
        {FUNC_KMAP, META | 'V', {backpage}              },
        {FUNC_KMAP, META | 'W', {copyregion}            },
        {FUNC_KMAP, META | 'X', {namedcmd}              },
        {FUNC_KMAP, META | 'Z', {quickexit}             },
        {FUNC_KMAP, META | 0x7F, {delbword}             },

#if     MSDOS
        {FUNC_KMAP, SPEC | CONTROL | '_', {forwhunt}    },
        {FUNC_KMAP, SPEC | CONTROL | 'S', {backhunt}    },
        {FUNC_KMAP, SPEC | 71, {gotobol}                },
        {FUNC_KMAP, SPEC | 72, {backline}               },
        {FUNC_KMAP, SPEC | 73, {backpage}               },
        {FUNC_KMAP, SPEC | 75, {back_grapheme}          },
        {FUNC_KMAP, SPEC | 77, {forw_grapheme}          },
        {FUNC_KMAP, SPEC | 79, {gotoeol}                },
        {FUNC_KMAP, SPEC | 80, {forwline}               },
        {FUNC_KMAP, SPEC | 81, {forwpage}               },
        {FUNC_KMAP, SPEC | 82, {insspace}               },
        {FUNC_KMAP, SPEC | 83, {forwdel}                },
        {FUNC_KMAP, SPEC | 115, {backword}              },
        {FUNC_KMAP, SPEC | 116, {forwword}              },
#if     WORDPRO
        {FUNC_KMAP, SPEC | 132, {gotobop}               },
        {FUNC_KMAP, SPEC | 118, {gotoeop}               },
#endif
        {FUNC_KMAP, SPEC | 84, {cbuf1}                  },
        {FUNC_KMAP, SPEC | 85, {cbuf2}                  },
        {FUNC_KMAP, SPEC | 86, {cbuf3}                  },
        {FUNC_KMAP, SPEC | 87, {cbuf4}                  },
        {FUNC_KMAP, SPEC | 88, {cbuf5}                  },
        {FUNC_KMAP, SPEC | 89, {cbuf6}                  },
        {FUNC_KMAP, SPEC | 90, {cbuf7}                  },
        {FUNC_KMAP, SPEC | 91, {cbuf8}                  },
        {FUNC_KMAP, SPEC | 92, {cbuf9}                  },
        {FUNC_KMAP, SPEC | 93, {cbuf10}                 },
        {FUNC_KMAP, SPEC | 117, {gotoeob}               },
        {FUNC_KMAP, SPEC | 119, {gotobob}               },
        {FUNC_KMAP, SPEC | 141, {gotobop}               },
        {FUNC_KMAP, SPEC | 145, {gotoeop}               },
        {FUNC_KMAP, SPEC | 146, {yank}                  },
        {FUNC_KMAP, SPEC | 147, {killregion}            },
#endif

#if     VT220
        {FUNC_KMAP, SPEC | '1', {fisearch}              }, /* VT220 keys   */
        {FUNC_KMAP, SPEC | '2', {yank}                  },
        {FUNC_KMAP, SPEC | '3', {killregion}            },
        {FUNC_KMAP, SPEC | '4', {setmark}               },
        {FUNC_KMAP, SPEC | '5', {backpage}              },
        {FUNC_KMAP, SPEC | '6', {forwpage}              },
        {FUNC_KMAP, SPEC | 'A', {backline}              },
        {FUNC_KMAP, SPEC | 'B', {forwline}              },
        {FUNC_KMAP, SPEC | 'C', {forw_grapheme}         },
        {FUNC_KMAP, SPEC | 'D', {back_grapheme}         },
        {FUNC_KMAP, SPEC | 'c', {metafn}                },
        {FUNC_KMAP, SPEC | 'd', {back_grapheme}         },
        {FUNC_KMAP, SPEC | 'e', {forwline}              },
        {FUNC_KMAP, SPEC | 'f', {gotobob}               },
        {FUNC_KMAP, SPEC | 'h', {help}                  },
        {FUNC_KMAP, SPEC | 'i', {cex}                   },
#endif

        {FUNC_KMAP, 0x7F, {backdel}                     },

        /* special internal bindings */
        {FUNC_KMAP,  SPEC | META | 'W', {wrapword }     }, /* called on word wrap */
        {FUNC_KMAP,  SPEC | META | 'C', {nullproc }     }, /*  every command input */
        {FUNC_KMAP,  SPEC | META | 'R', {nullproc }     }, /*  on file read */
        {FUNC_KMAP,  SPEC | META | 'X', {nullproc }     }, /*  on window change P.K. */

};

#endif  /* EBIND_H_ */
