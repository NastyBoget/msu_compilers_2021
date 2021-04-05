#define export exports
extern "C" {
#include <qbe/all.h>
}
#undef export

#include <stdio.h>
#include <iostream>
#include <set>
#include <map>
#include <deque>
#include <algorithm>

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
set<pair<size_t, int> > marked_instructions;

// множество, аналогичное множеству помеченных инструкций
// используется на шаге mark алгоритма
set<pair<size_t, int> > work_list;

std::map<Blk *, std::set<Blk *> > rdf;

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
            if ((blk->ins[i].op == Ocall) || (blk->ins[i].op == Ovacall)) {
                marked_instructions.insert(std::make_pair(blk->id, i));
                work_list.insert(std::make_pair(blk->id, i));
#ifdef DEBUG
                cout << "Call: " << blk->name << " " << blk->id << " " << i << endl;
#endif
                // инструкции передачи аргументов
                for (int i0 = i - 1; (i0 >= 0) && (isarg(blk->ins[i0].op)); i0--) {
                    marked_instructions.insert(std::make_pair(blk->id, i));
                    work_list.insert(std::make_pair(blk->id, i));
#ifdef DEBUG
                    cout << "Arguments: " << blk->name << " " << i0 << endl;
#endif
                }
                // инструкции записи в память
            } else if (isstore(blk->ins[i].op)) {
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

static int useful(Ins *i) {
    return 0;
}

map<Blk *, set<Blk *> > rb; // reachable blocks
void calcRB(Blk *blk) {
    static set<Blk *> used;
    set<Blk *> curr_rb;
    used.insert(blk);
    if (blk->s1 != nullptr) {
        if (rb.find(blk->s1) != rb.end()) {
            set<Blk *> &tmp_rb = rb[blk->s1];
            curr_rb.insert(tmp_rb.begin(), tmp_rb.end());
        } else if (used.find(blk->s1) == used.end()) {
            calcRB(blk->s1);
            set<Blk *> &tmp_rb = rb[blk->s1];
            curr_rb.insert(tmp_rb.begin(), tmp_rb.end());
        }
        curr_rb.insert(blk->s1);
    }
    if (blk->s2 != nullptr) {
        if (rb.find(blk->s2) != rb.end()) {
            set<Blk *> &tmp_rb = rb[blk->s2];
            curr_rb.insert(tmp_rb.begin(), tmp_rb.end());
        } else if (used.find(blk->s2) == used.end()) {
            calcRB(blk->s2);
            set<Blk *> &tmp_rb = rb[blk->s2];
            curr_rb.insert(tmp_rb.begin(), tmp_rb.end());
        }
        curr_rb.insert(blk->s2);
    }
    used.erase(blk);
    rb[blk] = curr_rb;
#ifdef DEBUG
    cout << "BLOCK\t" << blk->name << endl;
    cout << "\t";
    for (set<Blk *>::iterator it = curr_rb.begin(); it != curr_rb.end(); it++) {
        cout << (*it)->name << " ";
    }
    cout << endl;
#endif
}

set<Blk *> ret_blocks;
map<Blk *, set<Blk *> > post_dom;
map<Blk *, int> post_dom_sizes;
map<Blk *, Blk *> rev_iDom;

void findRets(Blk *blk) {
    static set<Blk *> used;
    static int level = 1;
    used.insert(blk);
    post_dom[blk] = set<Blk *>();
    post_dom_sizes[blk] = 0;
    if (blk->s1 != nullptr) {
#ifdef DEBUG
        std::cout << "BLOCK " << blk->name << " s1 " << blk->s1->name << endl;
#endif
        if (used.find(blk->s1) == used.end()) {
            level++;
            findRets(blk->s1);
            level--;
        } else {
#ifdef DEBUG
            std::cout << "\tUSED ALREADY" << endl;
#endif
        }
    }
    if (blk->s2 != nullptr) {
#ifdef DEBUG
        std::cout << "BLOCK " << blk->name << " s2 " << blk->s2->name << endl;
#endif
        if (used.find(blk->s2) == used.end()) {
            level++;
            findRets(blk->s2);
            level--;
        } else {
#ifdef DEBUG
            std::cout << "\tUSED ALREADY" << endl;
#endif
        }
    }
    if (blk->s1 == nullptr && blk->s2 == nullptr) {
        ret_blocks.insert(blk);
    }
#ifdef DEBUG
    cout << "============ REVERSED GRAPH ============" << endl;
    cout << "BLOCK\t" << blk->name << endl << "\t";
    for (int i = 0; i < blk->npred; ++i) {
        cout << blk->pred[i]->name << " ";
    }
    cout << endl;
    cout << "============ REVERSED GRAPH ============" << endl;
#endif
}

bool updateRDomSizes() {
    bool changed = false;
    for (map<Blk *, int>::iterator it = post_dom_sizes.begin(); it != post_dom_sizes.end(); it++) {
        if (post_dom[it->first].size() != it->second) {
            changed = true;
        }
        it->second = post_dom[it->first].size();
    }
    return changed;
}

void calcRDom(Blk* blk_start) {
    Blk* tmp_end = new Blk;
    snprintf(tmp_end->name, 9,"@FULLEND");
    tmp_end->s1 = tmp_end->s2 = nullptr;
    tmp_end->npred = ret_blocks.size();
    tmp_end->pred = new Blk* [tmp_end->npred];
    int tmp_it = 0;
    for (set<Blk*>::iterator el = ret_blocks.begin(); el != ret_blocks.end(); el++, tmp_it++) {
        tmp_end->pred[tmp_it] = *el;
        (*el)->s1 = tmp_end;
    }

    map<Blk *, set<Blk *> > used_all;
    set<Blk*> used_all1;

    for (set<Blk *>::iterator it = ret_blocks.begin(); it != ret_blocks.end(); ++it) {
        used_all[*it] = set<Blk *>();
    }
    int cnt = 0;
#ifdef DEBUG
    cout << "========== CalcRDOM iterations ==========" << endl;
#endif
    do {
        cnt++;
#ifdef DEBUG
        cout << "========== Iteration " << cnt << " ==========" << endl;
#endif
        for (set<Blk *>::iterator it = ret_blocks.begin(); it != ret_blocks.end(); ++it) {
#ifdef DEBUG
            cout << "Block\t" << tmp_end->name << endl;
#endif
            set<Blk *> used;
            deque<Blk *> q_blocks;
            q_blocks.push_back(tmp_end);

            while (!q_blocks.empty()) {
                set<Blk *> intersect_pred, s1, s2;

                Blk *curr_block = q_blocks.front();
                q_blocks.pop_front();

#ifdef DEBUG
                cout << "\tCurr " << curr_block->name << endl;
#endif

                used.insert(curr_block);
                used_all[tmp_end].insert(curr_block);
                used_all1.insert(curr_block);
                if (curr_block->s1 != nullptr && used_all1.find(curr_block->s1) != used_all1.end()) {
                    s1 = post_dom[curr_block->s1];
                    s1.insert(curr_block->s1);
                }
                if (curr_block->s2 != nullptr && used_all1.find(curr_block->s2) != used_all1.end()) {
                    s2 = post_dom[curr_block->s2];
                    s2.insert(curr_block->s2);
                }
                post_dom[curr_block].clear();
                if (curr_block->s1 != nullptr && curr_block->s2 != nullptr &&
                    used_all1.find(curr_block->s1) != used_all1.end() &&
                    used_all1.find(curr_block->s2) != used_all1.end()) {
                    set_intersection(s1.begin(), s1.end(), s2.begin(), s2.end(),
                                     inserter(intersect_pred, intersect_pred.begin()));
                    post_dom[curr_block].insert(intersect_pred.begin(), intersect_pred.end());
                } else if (curr_block->s1 != nullptr && used_all1.find(curr_block->s1) != used_all1.end()) {
                    post_dom[curr_block].insert(s1.begin(), s1.end());
                } else if (curr_block->s2 != nullptr && used_all1.find(curr_block->s2) != used_all1.end()) {
                    post_dom[curr_block].insert(s2.begin(), s2.end());
                }
                for (int i = 0; i < curr_block->npred; ++i) {
                    if (used.find(curr_block->pred[i]) == used.end()) {
                        q_blocks.push_back(curr_block->pred[i]);
                    }
                }
            }
        }
    } while (updateRDomSizes());
    for (set<Blk *>::iterator it = ret_blocks.begin(); it != ret_blocks.end(); ++it) {
        post_dom[(*it)].clear();
        (*it)->s1 = nullptr;
    }
    for (Blk* blk = blk_start; blk != nullptr; blk = blk->link) {
        post_dom[blk].erase(tmp_end);
    }
    post_dom.erase(tmp_end);
    delete[] tmp_end->pred;
    delete tmp_end;
#ifdef DEBUG
    cout << "============== calcRDom ==============" << endl;
    for (map<Blk *, set<Blk *> >::iterator it = post_dom.begin(); it != post_dom.end(); ++it) {
        cout << "BLOCK " << it->first->name << endl << "\t";
        for (set<Blk *>::iterator it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
            cout << (*it2)->name << " ";
        }
        cout << endl;
    }
    cout << "============== calcRDom ==============" << endl;
#endif
}

void calcRIDom() {
    for (map<Blk *, set<Blk *> >::iterator it = post_dom.begin(); it != post_dom.end(); it++) {
        rev_iDom[it->first] = nullptr;
        for (set<Blk *>::iterator curr_idom = it->second.begin(); curr_idom != it->second.end(); curr_idom++) {
            bool ok = true;
            for (set<Blk *>::iterator check_dom = it->second.begin(); check_dom != it->second.end(); check_dom++) {
                if (*curr_idom == *check_dom || *curr_idom == it->first || *check_dom == it->first)
                    continue;
                if (post_dom[*check_dom].find(*curr_idom) != post_dom[*check_dom].end()) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                rev_iDom[it->first] = *curr_idom;
            }
        }
    }
#ifdef DEBUG
    cout << "============== calcRIDom ==============" << endl;
    for (map<Blk *, Blk *>::iterator it = rev_iDom.begin(); it != rev_iDom.end(); ++it) {
        cout << "BLOCK " << it->first->name << "\tIDom " << ((it->second != nullptr) ? it->second->name : "null")
             << endl;
    }
    cout << "============== calcRIDom ==============" << endl;
#endif
}


void fillRdf(Blk *blk_start) {
    findRets(blk_start);
    calcRDom(blk_start);
#ifdef DEBUG
    cout << ">>>>>>>>>> FINISHED RDom <<<<<<<<<<" << endl;
#endif
    calcRIDom();
#ifdef DEBUG
    cout << ">>>>>>>>>> FINISHED RIDom <<<<<<<<<<" << endl;
#endif
    for (Blk *blk = blk_start; blk != nullptr; blk = blk->link) {
        rdf[blk] = set<Blk *>();

    }
    for (Blk *blk = blk_start; blk != nullptr; blk = blk->link) {
#ifdef DEBUG
        cout  << "BLOCK " << blk->name << " s1 " << ((blk->s1) ? blk->s1->name : "null") << " s2 " << ((blk->s2) ? blk->s2->name : "null") << endl;
#endif
        if (blk->s1 != nullptr && blk->s2 != nullptr) {
            Blk *r = blk->s1;
            while (r != rev_iDom[blk] && r != nullptr) {
                rdf[r].insert(blk);
                r = rev_iDom[r];
            }
            r = blk->s2;
            while (r != rev_iDom[blk] && r != nullptr) {
                rdf[r].insert(blk);
                r = rev_iDom[r];
            }
        }
    }

#ifdef DEBUG
    cout << "============== fillRdf ==============" << endl;
    for (map<Blk *, set<Blk *> >::iterator it = rdf.begin(); it != rdf.end(); ++it) {
        cout << "BLOCK\t" << it->first->name << endl << "\tDF\t";
        for (set<Blk *>::iterator df = it->second.begin(); df != it->second.end(); ++df) {
            cout << (*df)->name << " ";
        }
        cout << endl;
    }
    cout << "============== fillRdf ==============" << endl;
#endif
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
                } catch (const std::runtime_error &e) {
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
            } catch (const std::runtime_error &e) {
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
                } catch (const std::runtime_error &e) {
                    if (strcmp(e.what(), "No variable definition!") != 0) {
                        throw std::runtime_error(e.what());
                    }
                }
            }
        }

        for (std::set<Blk *>::iterator it = rdf[blk].begin(); it != rdf[blk].end(); ++it) {
            Blk *blk_j = FindByBlkId(fn, (*it)->id);
            std::pair<size_t, int> ins_j = std::make_pair((*it)->id, -1);
            if (marked_instructions.find(ins_j) == marked_instructions.end()) {
                marked_instructions.insert(ins_j);
                work_list.insert(ins_j);
            }
        }
    }
}

void Sweep(Fn *fn) {
    for (Blk *blk = fn->start; blk; blk = blk->link) {
        pair<size_t, int> inst = make_pair(blk->id, -1);
        if (marked_instructions.find(inst) == marked_instructions.end()) {
            Blk* RIDom = rev_iDom[blk];
            blk->jmp.type = Jjmp;
            blk->jmp.arg = R;
            blk->s1 = RIDom;
            blk->s2 = RIDom;
        }

        for (int i = 0; i < blk->nins; i++) {
            pair<size_t, int> inst = make_pair(blk->id, i);
            if (marked_instructions.find(inst) == marked_instructions.end()) {
                blk->ins[i].op = Onop;
                blk->ins[i].to = R;
                blk->ins[i].arg[0] = R;
                blk->ins[i].arg[1] = R;
            }
        }
    }
}

static void readfn(Fn *fn) {
    fillrpo(fn);
    fillpreds(fn);
    filluse(fn);
    ssa(fn);
#ifdef DEBUG
    printfn(fn, stdout);
#endif

    marked_instructions.clear();
    work_list.clear();

    fillRdf(fn->start);
    Mark(fn);
    Sweep(fn);
    
    fillpreds(fn);
    fillrpo(fn);
    printfn(fn, stdout);
}

static void readdat(Dat *dat) {
    (void) dat;
}

int main() {
    parse(stdin, (char *) "<stdin>", readdat, readfn);
    freeall();
}
