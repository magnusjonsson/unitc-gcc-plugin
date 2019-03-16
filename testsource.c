#define unit(u) __attribute((unit(#u)))

int error_return(int unit(m) x) { return x; }
int one(void) { return 1; }

int square_unitless(int x) { return x * x; }

int unit(m*m) square_with_unit(int unit(m) x) { return x * x; }

int unit(m) error_square_with_unit_1(int unit(m) x) { return x * x; }
int         error_square_with_unit_2(int unit(m) x) { return x * x; }

void error_assign (int a, int unit(m) b) { a = b; }
void error_compare(int a, int unit(m) b) { a == b; }

void error_pointer_assign (int *a, int unit(m) *b) { a = b; }
void error_pointer_compare(int *a, int unit(m) *b) { a == b; }

float unit(a) explicit_cast_repr_1(int unit(a) a) { return (float) a; }
float unit(a) implicit_cast_repr_2(int unit(a) a) { return a; }

double unit(m) cast_1_error(double a) { return (double unit(m)) a; }

int main(int argc, char **argv) {
  return 0;
}
