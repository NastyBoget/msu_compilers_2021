#define export exports
extern "C" {
#include <qbe/all.h>
}
#undef export

#include <stdio.h>
#include <string>
#include <set>

using namespace std;

// множество помеченных инструкций трех типов:
// 1) инструкции, описываемые структурой Ins (не являющиеся терминаторами) (blk->ins)
// 2) инструкции-терминаторы (безусловный переход, ветвление и возврат из функции) (blk->jmp)
// 3) фи-функции (blk->phi)
// элемент множества - пара <имя блока, номер инструкции в блоке>
// имя блока: blk->name
// номер инструкции: неотрицательное число для инструкций первого типа (индекс в массиве blk->ins),
// для инструкций-терминаторов номер равен -1 (в каждом блоке не более 1 перехода)
// для фи-функций нумерация от -2 в обратную сторону (номер фи-функции в списке blk->phi начиная с -2) -2, -3, ...
set< pair<string, int> > marked_instructions;

// множество, аналогичное множеству помеченных инструкций
// используется на шаге mark алгоритма
set< pair<string, int> > work_list;

// множество помеченных блоков программы для вычисления ближайшего помеченного постдоминатора
set<string> marked_blocks;

// заполнение множеств marked_instructions и work_list критическими инструкциями
// 1) инструкции, возвращающие значение из процедуры
// 2) инструкции ввода-вывода
// 3) инструкции, влияющие на значения в доступных извне процедуры областях памяти
static void find_critical_instructions(Fn *fn) {}

static int useful(Ins* i) {
    return 0;
}

static void readfn (Fn *fn) {
    fillrpo(fn);
    fillpreds(fn);
    filluse(fn);
    ssa(fn);

    for (Blk *blk = fn->start; blk; blk = blk->link) {
        for (Ins *i = blk->ins; i < &blk->ins[blk->nins]; ++i) {
            if (!useful(i)) {
                i->op = Onop;
                i->to = R;
                i->arg[0] = R;
                i->arg[1] = R;
            }
        }
        
    }

    fillpreds(fn);
    fillrpo(fn);
    printfn(fn, stdout);
}

static void readdat (Dat *dat) {
  (void) dat;
}

int main () {
  parse(stdin, (char *)"<stdin>", readdat, readfn);
  freeall();
}