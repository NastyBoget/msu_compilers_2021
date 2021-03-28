# Удаление бесполезного кода

### Задание

Реализуйте алгоритм удаления бесполезного кода для функций на языке промежуточного представления QBE.

Критическими считайте инструкции записи в память, инструкции возврата из функции и любые вызовы функций.

Решение должно считывать со стандартного потока ввода текстовый файл, содержащий ровно одну функцию на языке промежуточного представления QBE, а на стандартный поток вывода выдавать её же промежуточное представление после преобразования к SSA форме (с помощью libqbe) и удаления бесполезного кода.

Бесполезные инструкции следует заменять инструкциями nop без операндов.

### Замечания

* Структура [Ins](https://compilers.ispras.ru/doxygen/struct_ins.html) не описывает инструкции, встречающиеся на границах базовых блоков: [терминаторы (переходы)](https://compilers.ispras.ru/doxygen/struct_blk.html#a0880ee1f618cd7eeee56884f7b413505) и [фи-функции](https://compilers.ispras.ru/doxygen/struct_phi.html). Для описания обобщённых инструкций удобно использовать структуру [Use](https://compilers.ispras.ru/doxygen/struct_use.html) (поле bid в ней предназначено для хранения идентификатора блока, [идентификаторы блоков](https://compilers.ispras.ru/doxygen/struct_blk.html#a8c4d387a30655694a38b67c49b16a914) заполняются функцией fillrpo).

* Инструкции–терминаторы, в свою очередь, делятся на инструкции безусловного перехода (Jjmp), ветвления (Jjnz) и возврата (используйте макрос [isret](https://compilers.ispras.ru/doxygen/all_8h.html#a3e516be72a0201f9d22357aadf690e48) для проверки на принадлежность к данному классу).

* Т.к. функции могут принимать произвольное число аргументов, инструкции вызова в QBE не имеют операндов, вместо чего они предваряются нужным числом инструкций–аргументов (из-за того, что инструкция во внутреннем представлении QBE может иметь не более двух операндов). Cм. обработку инструкций Ocall и Ovacall [здесь](https://c9x.me/git/qbe.git/tree/amd64/sysv.c?id=f1c865f4bc7dff5a5d844049a73ad82463186e9f#n671).

* Неиспользуемые фи-функции нужно просто удалить (не заменять на nop).

### Документация QBE

* [Wiki](https://compilers.ispras.ru/wiki/Заглавная_страница)

* [Си-интерфейс](https://compilers.ispras.ru/doxygen/index.html)

### Установка библиотеки QBE (для справки, можно не делать)

```
git clone https://compilers.ispras.ru/git/qbe.git
cd qbe
sudo make && sudo make install DESTDIR=/usr
```