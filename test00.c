extern int GET();
extern int * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int a = 10;
int b = a;

int main() {
	int* p;
	int* q;
	int a;
	*(p+1) = 10;
	a = *(p+1);
	PRINT(a);

}
