extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int b=0;

int main() {
	int a=1;
	a=100;
	PRINT(a);
}
