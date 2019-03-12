;
;void DrawASpanLo(Word Count,LongWord xfrac,LongWord yfrac,Fixed ds_xstep,
;	Fixed ds_ystep,Byte *Dest)
;

	AREA	|C$$code|,CODE,READONLY
|x$codeseg|

	EXPORT DrawASpanLo
	IMPORT PlaneSource

	MACRO
	Filler
	LCLA	Foo
Foo	SETA	280/4
	WHILE	Foo/=0
	AND      v4,v1,a3,LSR #20		;v4 = first y index
	ORR      v4,v4,a2,LSR #26		;V4 = first x,y index
	ADD      a2,a2,a4
	ADD      a3,a3,v2

	AND      ip,v1,a3,LSR #20
	ORR      ip,ip,a2,LSR #26
	ADD      a2,a2,a4
	ADD      a3,a3,v2

	LDRB	v4,[v3,v4]
	LDRB	ip,[v3,ip]

	ORR		ip,ip,v4,LSL #16
	ORR		ip,ip,ip,LSL #8
	
	STR		ip,[lr],#4		;13 longs (52)

Foo	SETA	Foo-1
	WEND
	MEND
	
;
; Main entry point for the span code
;


SrcP DCD	PlaneSource	;Pointer to the source image

DrawASpanLo
	STMDB    sp!,{v1-v5,lr}
	MOV      a2,a2,LSL #10		;XFrac
    MOV		a3,a3,LSL #10		;YFrac
    MOV		a4,a4,LSL #11		;XStep
    ADD		lr,sp,#&18
    LDR		v3,SrcP
    LDR		v3,[v3]		;v3 = Src
    ADD		v3,v3,#64			;Adjust past the PLUT
    LDMIA	lr,{v2,lr}		;v2 = YStep, lr = Dest
    MOV		v2,v2,LSL #11	;YStep
    MOV		v1,#&fc0		;YMask
	RSB		ip,a1,#280		;Negate the index

	MOV		ip,ip,LSR #2	;Long word index
	MOV		a1,#52
	MUL		ip,a1,ip
	ADD		pc,pc,ip
	
	NOP
	
	Filler				;Perform the runfill

	LDMIA    sp!,{v1-v5,pc}

	END
