#ifndef AU88X0_EQ_H
#define AU88X0_EQ_H


typedef struct {
	u16 LeftCoefs[50];	
	u16 RightCoefs[50];	
	u16 LeftGains[10];	
	u16 RightGains[10];	
} auxxEqCoeffSet_t;

typedef struct {
	s32 this04;		
	s32 this08;		
} eqhw_t;

typedef struct {
	eqhw_t this04;		
	u16 this08;		
	u16 this0a;
	u16 this0c;		
	u16 this0e;

	s32 this10;		
	u16 this14_array[10];	
	s32 this28;		
	s32 this54;		
	s32 this58;
	s32 this5c;
	 auxxEqCoeffSet_t coefset;
	
	u16 this130[20];	
} eqlzr_t;

#endif
