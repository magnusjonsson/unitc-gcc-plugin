#define unit(u) __attribute((unit(#u)))

enum simple_enum { A, B };
enum complex_enum { C = 1, D = -3};
typedef enum { E, H } typedef_enum;

int length1 unit(meters);

int unit(meters) length2;

unit(meters) int length3;

int main(int argc, char **argv) {
  int x = argc;
  return x >= 2;
}
