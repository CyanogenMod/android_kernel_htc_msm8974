#ifndef _ALPHA_ERRNO_H
#define _ALPHA_ERRNO_H

#include <asm-generic/errno-base.h>

#undef	EAGAIN			

#define	EDEADLK		11	

#define	EAGAIN		35	
#define	EWOULDBLOCK	EAGAIN	
#define	EINPROGRESS	36	
#define	EALREADY	37	
#define	ENOTSOCK	38	
#define	EDESTADDRREQ	39	
#define	EMSGSIZE	40	
#define	EPROTOTYPE	41	
#define	ENOPROTOOPT	42	
#define	EPROTONOSUPPORT	43	
#define	ESOCKTNOSUPPORT	44	
#define	EOPNOTSUPP	45	
#define	EPFNOSUPPORT	46	
#define	EAFNOSUPPORT	47	
#define	EADDRINUSE	48	
#define	EADDRNOTAVAIL	49	
#define	ENETDOWN	50	
#define	ENETUNREACH	51	
#define	ENETRESET	52	
#define	ECONNABORTED	53	
#define	ECONNRESET	54	
#define	ENOBUFS		55	
#define	EISCONN		56	
#define	ENOTCONN	57	
#define	ESHUTDOWN	58	
#define	ETOOMANYREFS	59	
#define	ETIMEDOUT	60	
#define	ECONNREFUSED	61	
#define	ELOOP		62	
#define	ENAMETOOLONG	63	
#define	EHOSTDOWN	64	
#define	EHOSTUNREACH	65	
#define	ENOTEMPTY	66	

#define	EUSERS		68	
#define	EDQUOT		69	
#define	ESTALE		70	
#define	EREMOTE		71	

#define	ENOLCK		77	
#define	ENOSYS		78	

#define	ENOMSG		80	
#define	EIDRM		81	
#define	ENOSR		82	
#define	ETIME		83	
#define	EBADMSG		84	
#define	EPROTO		85	
#define	ENODATA		86	
#define	ENOSTR		87	

#define	ENOPKG		92	

#define	EILSEQ		116	

#define	ECHRNG		88	
#define	EL2NSYNC	89	
#define	EL3HLT		90	
#define	EL3RST		91	

#define	ELNRNG		93	
#define	EUNATCH		94	
#define	ENOCSI		95	
#define	EL2HLT		96	
#define	EBADE		97	
#define	EBADR		98	
#define	EXFULL		99	
#define	ENOANO		100	
#define	EBADRQC		101	
#define	EBADSLT		102	

#define	EDEADLOCK	EDEADLK

#define	EBFONT		104	
#define	ENONET		105	
#define	ENOLINK		106	
#define	EADV		107	
#define	ESRMNT		108	
#define	ECOMM		109	
#define	EMULTIHOP	110	
#define	EDOTDOT		111	
#define	EOVERFLOW	112	
#define	ENOTUNIQ	113	
#define	EBADFD		114	
#define	EREMCHG		115	

#define	EUCLEAN		117	
#define	ENOTNAM		118	
#define	ENAVAIL		119	
#define	EISNAM		120	
#define	EREMOTEIO	121	

#define	ELIBACC		122	
#define	ELIBBAD		123	
#define	ELIBSCN		124	
#define	ELIBMAX		125	
#define	ELIBEXEC	126	
#define	ERESTART	127	
#define	ESTRPIPE	128	

#define ENOMEDIUM	129	
#define EMEDIUMTYPE	130	
#define	ECANCELED	131	
#define	ENOKEY		132	
#define	EKEYEXPIRED	133	
#define	EKEYREVOKED	134	
#define	EKEYREJECTED	135	

#define	EOWNERDEAD	136	
#define	ENOTRECOVERABLE	137	

#define	ERFKILL		138	

#define EHWPOISON	139	

#endif
