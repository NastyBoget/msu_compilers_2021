export function w $bar() {
@start
      %x =w copy 1
      jnz %x, @d1, @d2
@d1
      %y =w copy 1
      %x =w mul %x, %y
      jmp @end
@d2
      %z = w copy 1
      jmp @end
@end
      ret %x
}
