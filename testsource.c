#define unit(u) __attribute((unit(#u)))

enum simple_enum { A, B };
enum complex_enum { C = 1, D = -3};
typedef enum { E, H } typedef_enum;

int length1 unit(meters);

int unit(meters) length2;

unit(meters) int length3;

double unit(meters * meters) area;

double unit(1) unitless1;

double unit(meters / meters) unitless2;

double one(void) {
  return 1;
}

double square(double x) {
  return x * x;
}

double unit(meters*meters) square_length(double unit(meters) x) {
  return x * x;
}

double unit(meters) square_length_error(double unit(meters) x) {
  return x * x;
}

int main(int argc, char **argv) {
  int x = argc;
  length1 = length2;
  area = length1 * length2;

  int b;
  b = (double) length1 == length2;
  b = (double unit(blah)) length1 == (double) length2;
  b = 0;
  return x >= 2;
}
