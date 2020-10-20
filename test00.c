extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int a = 10;
int b = a;

int main() {
	if(a == 11)
		PRINT(a);
	else
		PRINT(b);
}
