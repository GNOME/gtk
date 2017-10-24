uniform Foo {
  int x;
} x[2][3][4];

void main()
{
  bool t = x[1] == x[0];
}
