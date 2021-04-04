function w $baz(l %p, w %q) {
@start
      %x =w loadw %p
      %w =w mul %x, %x
      %c =w ceqw %w, %x
      jnz %c, @ret, @stor

@stor
      storew %w, %p
      %z =w copy %w
      jmp @ret2

@ret
      %z =w loadw %p
      %z =w add %z, %x
      ret %p

@ret2
      %z =w add %q, %q
      ret %q
}
