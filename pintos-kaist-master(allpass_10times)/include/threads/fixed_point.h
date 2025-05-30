
#define F (1 << 14)
#define INT_MAX ((1 << 31) - 1)
#define INT_MIN (-(1 << 31))

int convert_fixed(int n){//정수를 변환
	return n*F;
}
int convert_int(int x){//실수를 변환
	return x/F;
}
int convert_int_round(int x){
	if(x<=0){
		return (x-F/2)/F;
	}else 
	{
		return (x+F/2)/F;
	}
	
}
int add_fp(int x,int y){//고정소수점끼리
	return x+y;
}
int add_fpn(int x,int n){
	return x+n*F;
}
int sub_fp(int x,int y){//고정소수점끼리
	return x-y;
}
int sub_fpn(int x,int n){
	return x-n*F;
}
int mult_fp(int x,int y){//고정소수점끼리
	return ((int64_t)x)*y/F;
}
int mult_fpn(int x,int n){
	return x*n;
}
int div_fp(int x,int y){//고정소수점끼리
	return ((int64_t)x)*F/y;
}
int div_fpn(int x,int n){
	return x/n;
}