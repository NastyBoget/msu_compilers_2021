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
// 1) инструкции вызова функций и передачи аргументов в них +
// 2) инструкции записи в память +
// 3) инструкции, возвращающие значение из функции
// 4) фи-функции
static void findCriticalInstructions(Fn *fn) {
    for (Blk *blk = fn->start; blk != nullptr; blk = blk->link) {
        for (int i = 0; i < blk->nins; ++i) {
            Ins *ins = &blk->ins[i];
            // инструкция вызова функции
            if ((blk->ins[i].op == Ocall) || (blk->ins[i].op == Ovacall))
            {
                marked_instructions.insert(std::make_pair(blk->id, i));
                work_list.insert(std::make_pair(blk->id, i));
#ifdef DEBUG
                cout << "Call: " << blk->name << " " << blk->id << " " << i << endl;
#endif
            // инструкции передачи аргументов
                for (int i0 = i - 1; (i0 >= 0) && (isarg(blk->ins[i0].op)); i0--)
                {
                    marked_instructions.insert(std::make_pair(blk->id, i));
                    work_list.insert(std::make_pair(blk->id, i));
#ifdef DEBUG
                    cout << "Arguments: " << blk->name << " " << i0 << endl;
#endif
                }
            // инструкции записи в память
            }
            else if (isstore(blk->ins[i].op))
            {
                marked_instructions.insert(std::make_pair(blk->id, i));
                work_list.insert(std::make_pair(blk->id, i));
#ifdef DEBUG
                cout << "Memory: " << blk->name << " " << i << endl;
#endif
            }
        }
        if (isret(blk->jmp.type)) {
            marked_instructions.insert(std::make_pair(blk->id, -1));
            work_list.insert(std::make_pair(blk->id, -1));
#ifdef DEBUG
            cout << "Return: " << blk->name << endl;
#endif
        }
    }
}

static int useful(Ins* i) {
    return 0;
}

std::pair<size_t, int> Def(Fn *fn, Ref arg) {
    for (Blk *blk = fn->start; blk != nullptr; blk = blk->link) {
        for (size_t i = 0; i < blk->nins; ++i) {
            Ins *ins = &blk->ins[i];
            if (req(ins->to, arg)) {
                return std::make_pair(blk->id, i);
            }
        }
        Phi *phi = blk->phi;
        for (int i = -2; phi != nullptr; --i, phi = phi->link) {
            if (req(phi->to, arg)) {
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

void Mark(Fn *fn) {

    findCriticalInstructions(fn);

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
#ifdef DEBUG
                        cout << "From instruction: " << FindByBlkId(fn, def.first)->name << " " << def.second << endl;
#endif
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
#ifdef DEBUG
                    cout << "From jump: " << FindByBlkId(fn, def.first)->name << " " << def.second << endl;
#endif
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
#ifdef DEBUG
                        cout << "From phi: " << FindByBlkId(fn, def.first)->name << " " << def.second << endl;
#endif
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
}

static void readfn (Fn *fn) {
    fillrpo(fn);
    fillpreds(fn);
    filluse(fn);
    ssa(fn);

#ifdef DEBUG
    printfn(fn, stdout);
#endif

    marked_instructions.clear();
    work_list.clear();

    Mark(fn);

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
