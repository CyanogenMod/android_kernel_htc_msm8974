#ifndef _PARISC_ERRNO_H
#define _PARISC_ERRNO_H

#include <asm-generic/errno-base.h>

#define	ENOMSG		35	
#define	EIDRM		36	
#define	ECHRNG		37	
#define	EL2NSYNC	38	
#define	EL3HLT		39	
#define	EL3RST		40	
#define	ELNRNG		41	
#define	EUNATCH		42	
#define	ENOCSI		43	
#define	EL2HLT		44	
#define	EDEADLK		45	
#define	EDEADLOCK	EDEADLK
#define	ENOLCK		46	
#define	EILSEQ		47	

#define	ENONET		50	
#define	ENODATA		51	
#define	ETIME		52	
#define	ENOSR		53	
#define	ENOSTR		54	
#define	ENOPKG		55	

#define	ENOLINK		57	
#define	EADV		58	
#define	ESRMNT		59	
#define	ECOMM		60	
#define	EPROTO		61	

#define	EMULTIHOP	64	

#define	EDOTDOT		66	
#define	EBADMSG		67	
#define	EUSERS		68	
#define	EDQUOT		69	
#define	ESTALE		70	
#define	EREMOTE		71	
#define	EOVERFLOW	72	


#define	EBADE		160	
#define	EBADR		161	
#define	EXFULL		162	
#define	ENOANO		163	
#define	EBADRQC		164	
#define	EBADSLT		165	
#define	EBFONT		166	
#define	ENOTUNIQ	167	
#define	EBADFD		168	
#define	EREMCHG		169	
#define	ELIBACC		170	
#define	ELIBBAD		171	
#define	ELIBSCN		172	
#define	ELIBMAX		173	
#define	ELIBEXEC	174	
#define	ERESTART	175	
#define	ESTRPIPE	176	
#define	EUCLEAN		177	
#define	ENOTNAM		178	
#define	ENAVAIL		179	
#define	EISNAM		180	
#define	EREMOTEIO	181	
#define	ENOMEDIUM	182	
#define	EMEDIUMTYPE	183	
#define	ENOKEY		184	
#define	EKEYEXPIRED	185	
#define	EKEYREVOKED	186	
#define	EKEYREJECTED	187	


#define ENOSYM		215	
#define	ENOTSOCK	216	
#define	EDESTADDRREQ	217	
#define	EMSGSIZE	218	
#define	EPROTOTYPE	219	
#define	ENOPROTOOPT	220	
#define	EPROTONOSUPPORT	221	
#define	ESOCKTNOSUPPORT	222	
#define	EOPNOTSUPP	223	
#define	EPFNOSUPPORT	224	
#define	EAFNOSUPPORT	225	
#define	EADDRINUSE	226	
#define	EADDRNOTAVAIL	227	
#define	ENETDOWN	228	
#define	ENETUNREACH	229	
#define	ENETRESET	230	
#define	ECONNABORTED	231	
#define	ECONNRESET	232	
#define	ENOBUFS		233	
#define	EISCONN		234	
#define	ENOTCONN	235	
#define	ESHUTDOWN	236	
#define	ETOOMANYREFS	237	
#define EREFUSED	ECONNREFUSED	
#define	ETIMEDOUT	238	
#define	ECONNREFUSED	239	
#define EREMOTERELEASE	240	
#define	EHOSTDOWN	241	
#define	EHOSTUNREACH	242	

#define	EALREADY	244	
#define	EINPROGRESS	245	
#define	EWOULDBLOCK	246	
#define	ENOTEMPTY	247	
#define	ENAMETOOLONG	248	
#define	ELOOP		249	
#define	ENOSYS		251	

#define ENOTSUP		252	
#define ECANCELLED	253	
#define ECANCELED	ECANCELLED	

#define EOWNERDEAD	254	
#define ENOTRECOVERABLE	255	

#define	ERFKILL		256	

#define EHWPOISON	257	

#endif
