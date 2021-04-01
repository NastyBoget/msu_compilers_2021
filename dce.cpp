#define export exports
extern "C" {
#include <qbe/all.h>
}
#undef export

#include <stdio.h>
#include <iostream>
#include <set>
#include <map>

using namespace std;

// множество помеченных инструкций трех типов:
// 1) инструкции, описываемые структурой Ins (не являющиеся терминаторами) (blk->ins)
// 2) инструкции-терминаторы (безусловный переход, ветвление и возврат из функции) (blk->jmp)
// 3) фи-функции (blk->phi)
// элемент множества - пара <id блока, номер инструкции в блоке>
// id блока: blk->id
// номер инструкции: неотрицательное число для инструкций первого типа (индекс в массиве blk->ins),
// для инструкций-терминаторов номер равен -1 (в каждом блоке не более 1 перехода)
// для фи-функций нумерация от -2 в обратную сторону (номер фи-функции в списке blk->phi начиная с -2) -2, -3, ...
set< pair<size_t, int> > marked_instructions;

// множество, аналогичное множеству помеченных инструкций
// используется на шаге mark алгоритма
set< pair<size_t, int> > work_list;

std::map< size_t, std::set<size_t> > rdf;

// множество помеченных блоков программы для вычисления ближайшего помеченного постдоминатора
set<size_t> marked_blocks;

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

std::pair<size_t, int> Def(Fn *fn, Ref arg) {
    for (Blk *blk = fn->start; blk != nullptr; blk = blk->link) {
        for (size_t i = 0; i < blk->nins; ++i) {
            Ins *ins = &blk->ins[i];
            if (req(ins->to, arg)) {
                return std::make_pair(blk->id, i);
            }
        }
    }
    throw std::runtime_error("No variable definition!");
}

Blk *FindByBlkId(Fn *fn, size_t blk_id) {
    for (Blk *blk = fn->start; blk != nullptr; blk = blk->link) {
        if (blk->id == blk_id) {
            return blk;
        }
    }
    return nullptr;
}

Ins *FindByInsId(Fn *fn, Blk *blk, int ins_id) {
    (void) fn;
    if (ins_id >= 0 && ins_id < blk->nins) {
        return &blk->ins[ins_id];
    }
    return nullptr;
}

Phi *FindByPhiId(Fn *fn, Blk *blk, int phi_id) {
    (void) fn;
    if (phi_id < -1) {
        Phi *phi = blk->phi;
        for (int i = -2; phi != nullptr; --i, phi = phi->link) {
            if (i == phi_id) {
                return phi;
            }
        }
    }
    return nullptr;
}

bool IsCriticalIns(Fn *fn, Ins *ins) {
    (void) fn;
    (void) ins;
    return true; // FIXME!
}

bool IsCriticalPhi(Fn *fn, Phi *phi) {
    (void) fn;
    (void) phi;
    return true; // FIXME!
}

bool IsCriticalJmp(Fn *fn, Blk *blk) {
    (void) fn;
    (void) blk;
    return true; // FIXME!
}

void Mark(Fn *fn) {
    fillrpo(fn);
    fillpreds(fn);
    filluse(fn);
    ssa(fn);

    marked_instructions.clear();
    work_list.clear();

    for (Blk *blk = fn->start; blk != nullptr; blk = blk->link) {
        for (int i = 0; i < blk->nins; ++i) {
            Ins *ins = &blk->ins[i];
            if (IsCriticalIns(fn, ins)) {
                marked_instructions.insert(std::make_pair(blk->id, i));
                work_list.insert(std::make_pair(blk->id, i));
            }
        }
        if (IsCriticalJmp(fn, blk)) {
            marked_instructions.insert(std::make_pair(blk->id, -1));
            work_list.insert(std::make_pair(blk->id, -1));
        }
        Phi *phi = blk->phi;
        for (int i = -2; phi != nullptr; --i, phi = phi->link) {
            if (IsCriticalPhi(fn, phi)) {
                marked_instructions.insert(std::make_pair(blk->id, i));
                work_list.insert(std::make_pair(blk->id, i));
            }
        }
    }

    while (!work_list.empty()) {
        size_t blk_id = work_list.begin()->first;
        int ins_id = work_list.begin()->second;

        work_list.erase(work_list.begin());

        Blk *blk = FindByBlkId(fn, blk_id);

        if (ins_id >= 0) {
            Ins *ins = FindByInsId(fn, blk, ins_id);

            for (size_t i = 0; i < 2; ++i) {
                try {
                    std::pair<size_t, int> def = Def(fn, ins->arg[i]);
                    if (marked_instructions.find(def) == marked_instructions.end()) {
                        marked_instructions.insert(def);
                        work_list.insert(def);
                    }
                } catch (const std::runtime_error& e) {
                    if (strcmp(e.what(), "No variable definition!") != 0) {
                        throw std::runtime_error(e.what());
                    }
                }
            }
        } else if (ins_id == -1) {
            try {
                std::pair<size_t, int> def = Def(fn, blk->jmp.arg);
                if (marked_instructions.find(def) == marked_instructions.end()) {
                    marked_instructions.insert(def);
                    work_list.insert(def);
                }
            } catch (const std::runtime_error& e) {
                if (strcmp(e.what(), "No variable definition!") != 0) {
                    throw std::runtime_error(e.what());
                }
            }
        } else {
            Phi *phi = FindByPhiId(fn, blk, ins_id);

            for (size_t i = 0; i < phi->narg; ++i) {
                try {
                    std::pair<size_t, int> def = Def(fn, phi->arg[i]);
                    if (marked_instructions.find(def) == marked_instructions.end()) {
                        marked_instructions.insert(def);
                        work_list.insert(def);
                    }
                } catch (const std::runtime_error& e) {
                    if (strcmp(e.what(), "No variable definition!") != 0) {
                        throw std::runtime_error(e.what());
                    }
                }
            }
        }

        for (std::set<size_t>::iterator it = rdf[blk->id].begin(); it != rdf[blk->id].end(); ++it) {
            Blk *blk_j = FindByBlkId(fn, *it);
            std::pair<size_t, int> ins_j = std::make_pair(*it, -1);
            if (marked_instructions.find(ins_j) == marked_instructions.end()) {
                marked_instructions.insert(ins_j);
                work_list.insert(ins_j);
            }
        }
    }

    fillpreds(fn);
    fillrpo(fn);
    printfn(fn, stdout);
}

int main () {
  parse(stdin, (char *)"<stdin>", readdat, Mark);
  freeall();
}
