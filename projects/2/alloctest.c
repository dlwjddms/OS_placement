#include <stdio.h>

void alloctest_NF(void);
void alloctest_BF(void);
void alloctest_BUDDY(void);
void alloctest(void){


//	alloctest_NF();
//	alloctest_BF();
	alloctest_BUDDY();
}

void alloctest_NF(void){

	/* nextfit testcode */
        char * mem[5];

	for(size_t i=0;i<5;i++ ){
		mem[i] = (char *)malloc(2048*6);
		ASSERT(mem[i]);
		memset(mem[i], 0X00, 2048*6);
		palloc_get_status(0);
	}

	free(mem[1]);
	palloc_get_status(0);

	mem[1] = (char *)malloc(2048*6);
	ASSERT(mem[1]);
	memset(mem[1], 0X00, 2048*6);
	palloc_get_status(0);



}
void alloctest_BUDDY(void){

	/* buddy testcode */
        char * mem[7];

	for(size_t i=0;i<7;i++ ){
		mem[i] = (char *)malloc(2048*6);
		ASSERT(mem[i]);
		memset(mem[i], 0X00, 2048*6);
		palloc_get_status(0);
	}
	free(mem[1]);
	palloc_get_status(0);

	free(mem[2]);
	palloc_get_status(0);

	free(mem[4]);
	palloc_get_status(0);

	free(mem[5]);
	palloc_get_status(0);

	mem[1] = (char *)malloc(2048*6);
	ASSERT(mem[1]);
	memset(mem[1], 0X00, 2048*6);
	palloc_get_status(0);


}
void alloctest_BF(void){
	
	/* bestfit testcode */
        char * mem[6];
     	mem[0] = (char *)malloc(2048*10);
	ASSERT(mem[0]);
	memset(mem[0], 0X00, 2048*10);
	palloc_get_status(0);
     	
	mem[1] = (char *)malloc(2048*10);
	ASSERT(mem[1]);
	memset(mem[1], 0X00, 2048*10);
	palloc_get_status(0);

	mem[2] = (char *)malloc(2048*10);
	ASSERT(mem[2]);
	memset(mem[2], 0X00, 2048*10);
	palloc_get_status(0);

	mem[3] = (char *)malloc(2048*14);
	ASSERT(mem[3]);
	memset(mem[3], 0X00, 2048*14);
	palloc_get_status(0);


	mem[4] = (char *)malloc(2048*6);
	ASSERT(mem[4]);
	memset(mem[4], 0X00, 2048*6);
	palloc_get_status(0);

	mem[5] = (char *)malloc(2048*10);
	ASSERT(mem[5]);
	memset(mem[5], 0X00, 2048*10);
	palloc_get_status(0);

	free(mem[1]);
	palloc_get_status(0);


	free(mem[4]);
	palloc_get_status(0);

	mem[4] = (char *)malloc(2048*6);
	ASSERT(mem[4]);
	memset(mem[4], 0X00, 2048*6);
	palloc_get_status(0);



	

}

