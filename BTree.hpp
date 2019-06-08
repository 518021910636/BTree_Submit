//
// Created by peter_zheng on 2019-03-09.
//

#include "utility.hpp"
#include <functional>
#include <cstddef>
#include "exception.hpp"
#include<cstring>
#include <map>
#include <c++/iostream>
#include<fstream>

namespace sjtu {
    template <class Key, class Value, class Compare = std::less<Key> >
    class BTree {
    public:
        typedef pair<Key, Value> value_type;
        class iterator;
        class const_iterator;

    private:
        // Your private members go here
        static const int MAXN = 4;  /// node num max
        static const int MAXL = 4;  /// leaf values num max
        //static const int MINN = MAXN / 2;
        //static const int MINL = MAXL / 2;

        struct Name{
            char *str;
            Name(){str = new char[7];}
            ~Name(){ if(str != NULL)  delete str; }
            void Naming(const char *oth){ strcpy(str, oth); }
        };
        struct TREE{
            size_t head;
            size_t rear;
            size_t root;
            size_t size;
            size_t eof;
            TREE(){
                head = rear = root = size = eof = 0;
            }
        };
        struct Leaf{
            size_t offset;
            size_t parent;
            size_t previous;
            size_t next;
            int num;
            value_type data[MAXL + 1];
            Leaf(){ offset = parent = previous = next = num = 0; }
        };
        struct Node{
            size_t offset;
            size_t parent;
            size_t ch[MAXN + 1];
            Key key[MAXN + 1];
            int num;
            bool type;
            Node(){
                offset = parent = num = 0;
                type = 0;
                for(int i = 0; i <= MAXN; ++i)  ch[i] = 0;
            }
        };

        static const size_t leaf_size = sizeof(Leaf);
        static const size_t node_size = sizeof(Node);
        static const size_t tree_size = sizeof(TREE);

        FILE *fp;
        bool fileopen;
        bool file_exit;
        Name filename;
        TREE tree;

        ///file

        inline void openFile(){
            file_exit = 1;
            if(fileopen == 0){
                fp = fopen(filename.str, "rb+"); ///二进制打开
                if(fp == NULL){
                    file_exit = 0;
                    fp = fopen(filename.str, "w"); ///创建
                    fclose(fp);
                    fp = fopen(filename.str, "rb+");///以二进制开文件
                }
                else{
                    readFile(&tree, 0, 1, sizeof(TREE));  ///重新打开（一般不会调用）
                }
                fileopen = 1;
            }
        }
        inline void closeFile(){
            if(fileopen == 1){
                fclose(fp);
                fileopen = 0;
            }
        }
        inline void readFile(void *place, size_t offset, size_t n, size_t size) const{
            if(fseek(fp, offset, SEEK_SET) != 0)  throw "open file failed";///将fp从开头开始移动指向offset位置
            fread(place, size, n, fp); ///在fp处将place后面的n个大小为size的值写入
        }
        inline void writeFile(void *place, size_t offset, size_t n, size_t size){
            if(fseek(fp, offset, SEEK_SET) != 0)  throw "open file failed";
            fwrite(place, size, n, fp);
        }

        Name oldname;
        FILE *oldfp;

        inline void copy_readFile(void *place, size_t offset, size_t n, size_t size) const{
            if(fseek(oldfp, offset, SEEK_SET) != 0)  throw "open file failed";
            size_t ret = fread(place, n, size, oldfp);
        }

        size_t leaf_offset_tmp; /// 前一个叶子
        inline void copy_leaf(size_t offset, size_t from_offset, size_t par_offset){
            Leaf leaf, leaf_from, previous_leaf;
            copy_readFile(&leaf_from, from_offset, 1, leaf_size);
            leaf.offset = offset, leaf.parent = par_offset;///新位
            leaf.num = leaf_from.num; leaf.previous = leaf_offset_tmp; leaf.next = 0;
            if(leaf_offset_tmp != 0) {
                readFile(&previous_leaf, leaf_offset_tmp, 1, leaf_size);
                previous_leaf.next = offset;  ///关键，换当前leaf的位
                writeFile(&previous_leaf, leaf_offset_tmp, 1, leaf_size);
                tree.rear = offset;
            }/// 非首叶子
            else  tree.head = offset;
            for(int i = 0; i < leaf.num; ++i) {
                leaf.data[i].first = leaf_from.data[i].first;
                leaf.data[i].second = leaf_from.data[i].second;
            }
            writeFile(&leaf, offset, 1, leaf_size);
            tree.eof += leaf_size;
            leaf_offset_tmp = offset;
        }
        inline void copy_node(size_t offset, size_t from_offset, size_t par_offset){
            Node node, node_from;
            copy_readFile(&node_from, from_offset, 1,node_size);
            writeFile(&node, offset, 1, node_size); ///node指到修改的offset
            tree.eof += node_size;
            node.offset = offset;
            node.parent = par_offset;
            node.num = node_from.num;
            node.type = node_from.type;
            for(int i = 0; i < node.num; ++i){
                node.key[i] = node_from.key[i];
                if(node.type == 1){
                    copy_leaf(tree.eof, node_from.ch[i], offset);
                }///该函数操作的node已经递归到了叶子节点
                else{
                    copy_node(tree.eof, node_from.ch[i], offset);
                }///对儿子中间节点递归的移动
            }
            writeFile(&node, offset, 1, node_size);
        }
        inline void copyFile(char *tar, char *from){
            oldname.Naming(from);
            oldfp  = fopen(oldname.str, "+rb+");
            if(oldfp == NULL)  throw "no such file";
            TREE oldtree;
            copy_readFile(&oldtree, 0, 1, tree_size); /// 读入旧的basic_tree
            leaf_offset_tmp = 0;
            tree.size = oldtree.size;
            tree.root = tree.eof = tree_size;///加在basic_tree之后
            writeFile(&tree, 0, 1, tree_size);
            copy_node(tree.root, oldtree.root, 0);
            writeFile(&tree, 0, 1, tree_size);
            fclose(oldfp);
        }

        inline void build_tree(){
            Node root;
            Leaf leaf;
            tree.size = 0;
            tree.eof += tree_size + node_size + leaf_size;
            tree.root = root.offset = tree_size;
            tree.head = tree.rear = leaf.offset = tree_size + node_size;
            root.parent =  0;  root.type = 1;  root.num = 1;
            root.ch[0] = leaf.offset;
            leaf.parent = root.offset;
            leaf.num = leaf.next = leaf.previous = 0;
            writeFile(&tree, 0, 1, tree_size);
            writeFile(&root, root.offset, 1, node_size);
            writeFile(&leaf, leaf.offset, 1, leaf_size);
            std::cout<<"build_succuss\n";
        }
        ///返回叶子的offset
        size_t locate_leaf(const Key &key, size_t offset){
            Node node;
            readFile(&node, offset, 1, node_size);
            if(node.type == 1){
                int i = 0;
                for( ; i < node.num; ++i){
                    if(node.key[i] > key)  break;
                }
                if(i == 0)  return 0;
                ///从root开始找，node.key[0]一定是最小值，
                ///若key==node.key[0],则返回0；否则不在其中，但也随便返回一个0
                return node.ch[i - 1];
            }
            else{
                int i = 0;
                for( ; i< node.num; ++i){
                    if(node.key[i] > key)  break;
                }
                if(i == 0)  return 0;
                return locate_leaf(key, node.ch[i - 1]);
            }
        }
        ///已经先找到在那个叶子节点，用引用传入，返回迭代器指向插入元素所在的place
        pair<iterator, OperationResult>insert_leaf(Leaf &leaf, const Key &key, const Value &value){
            int pos = 0;
            for( ; pos < leaf.num; ++pos) {
                if (leaf.data[pos].first > key) break;
            }
            for(int i = leaf.num - 1; i >= pos; --i){
                leaf.data[i + 1].first = leaf.data[i].first;
                leaf.data[i + 1].second = leaf.data[i].second;
            }
            leaf.data[pos].first = key;
            leaf.data[pos].second = value;
            ++leaf.num;  ++tree.size;
            if(pos == 0){  ///在叶子节点的第一位插入，修改父亲的关键字值
                Node node;
                readFile(&node, leaf.parent, 1, node_size);
                node.key[0] = key;
                writeFile(&node, node.offset, 1, node_size);
            }
            iterator itr(this, leaf.offset, pos);
            writeFile(&tree, 0, 1, tree_size);
            if(leaf.num <= MAXL) writeFile(&leaf, leaf.offset, 1, leaf_size);
            else split_leaf(leaf, itr, key);///大于最大值，将叶子分裂，此时先不写入，并把迭代器也以引用形式放入函数中
            return pair<iterator, OperationResult> (itr, Success);///就算是Fail也从来不会承认
        }
        void insert_node(Node &node, const Key &key, size_t ch){
            int i = 0;
            for( ; i < node.num; ++i){///找到右边的第一个key
                if(node.key[i] > key)  break;
            }
            for(int j = node.num - 1; j >= i; --j){
                node.key[j + 1] = node.key[j];
                node.ch[j + 1] = node.ch[j];
            }
            node.key[i] = key;
            node.ch[i] = ch;
            ++node.num;
            if(node.num <= MAXN){
                writeFile(&node, node.offset, 1, node_size);
            }
            else split_node(node);  ///爸爸node也超范围--node分裂
        }
        void split_leaf(Leaf &leaf, iterator &itr, const Key &key){  ///在itr指的位置插进一个（还没插）
            Leaf newleaf;
            newleaf.num = leaf.num - (leaf.num >> 1);///新叶子：n-(n/2)
            leaf.num >>= 1;///老叶子 n/2
            newleaf.offset = tree.eof;///找个空位放一下
            tree.eof += leaf_size;
            newleaf.parent = leaf.parent;
            for(int i = 0; i < newleaf.num; ++i){
                newleaf.data[i].first = leaf.data[i + leaf.num].first, newleaf.data[i].second = leaf.data[i + leaf.num].second;
                if(newleaf.data[i].first == key){
                    itr.offset = newleaf.offset;
                    itr.place = i;
                }///只有当itr的offset和place位于newleaf中才要修改，否则在leaf中的话已经在insert_leaf中将itr设置好了
            }
            newleaf.next = leaf.next;
            newleaf.previous = leaf.offset;
            leaf.next = newleaf.offset;
            Leaf nextleaf;
            if(newleaf.next == 0){ tree.rear = newleaf.offset; }
            /// 在叶子表的最后一个叶子,tree尾巴就是原来的leaf
            else{  ///在叶子链表中间插入

                readFile(&nextleaf, newleaf.next, 1, leaf_size);
                nextleaf.previous = newleaf.offset;///拿出来改下一个叶子的前继指针
                writeFile(&nextleaf, nextleaf.offset, 1, leaf_size);
            }
            writeFile(&leaf, leaf.offset, 1, leaf_size);
            writeFile(&newleaf, newleaf.offset, 1, leaf_size);
            writeFile(&tree, 0, 1, tree_size);
            Node par;  ///在父亲节点把newleaf的首个key插入
            readFile(&par, leaf.parent, 1, node_size);
            insert_node(par, newleaf.data[0].first, newleaf.offset);
        }
        void split_node(Node &node){
            Node newnode;
            newnode.num = node.num - (node.num >> 1);//n = n - (n/2)
            node.num >>= 1;// n / 2
            newnode.parent = node.parent;
            newnode.type = node.type;
            newnode.offset = tree.eof;
            tree.eof += node_size;
            for(int i = 0; i < newnode.num; ++i){
                newnode.key[i] = node.key[i + node.num];
                newnode.ch[i] = node.ch[i + node.num];
            }
            if (newnode.type == 1){ ///直接连叶子，取出每个叶子改爸爸
                for(int i = 0; i < newnode.num; ++i){
                    Leaf leaf;
                    readFile(&leaf, newnode.ch[i], 1, leaf_size);
                    leaf.parent = newnode.offset;
                    writeFile(&leaf, leaf.offset, 1, leaf_size);
                }
            }
            else{  ///不直接连叶子，取出每个儿子改爸爸
                for(int i = 0; i < newnode.num; ++i){
                    Node tmpnode;
                    readFile(&tmpnode, newnode.ch[i], 1, node_size);
                    tmpnode.parent = newnode.offset;
                    writeFile(&tmpnode, tmpnode.offset, 1, node_size);
                }
            }

            if(node.offset == tree.root){  ///向上调用到对根节点修改，则make新根，且左右孩子是原根分裂成两个
                Node newroot;
                newroot.parent = newroot.type = 0;
                newroot.offset = tree.eof;
                tree.eof += node_size;
                newroot.num = 2;
                newroot.key[0] = node.key[0];
                newroot.ch[0] = node.offset;
                newroot.key[1] = newnode.key[0];
                newroot.ch[1] = newnode.offset;  ///不要打错啊！！白白查了一天
                node.parent = newnode.parent = newroot.offset;
                tree.root = newroot.offset;
                writeFile(&tree, 0, 1, tree_size);///文件头写入，将原根覆盖
                writeFile(&node, node.offset, 1, node_size);
                writeFile(&newnode, newnode.offset, 1, node_size);
                writeFile(&newroot, newroot.offset, 1, node_size);
            }
            else{
                writeFile(&tree, 0, 1, tree_size);
                writeFile(&node, node.offset, 1, node_size);
                writeFile(&newnode, newnode.offset, 1, node_size);
                Node parent;
                readFile(&parent, node.parent, 1, node_size);
                insert_node(parent, newnode.key[0], newnode.offset);
            }
        }



    public:

        class const_iterator;
        ///offset:迭代器指向的元素所在的叶节点的开头文件中位置
        /// place：在节点中的第几个（从0开始）
        /// Btree *point：目前所指的树，其实可以不加，毕竟只有一颗树
        class iterator {
            friend class BTree;
        private:
            // Your private members go here
            BTree *point;
            size_t offset;
            int place;
        public:
            bool modify(const Value& value){
                Leaf p;
                point->readFile((&p, offset, 1, leaf_size));
                p.data[place].second = value;
                point->writeFile(&p, offset, 1, leaf_size);
                return 1;
            }
            iterator() {
                point = nullptr;
                place = offset = 0;
            }
            iterator(BTree *bt, size_t off, size_t pl){
                point = bt;
                offset = off;
                place = pl;
            }
            iterator(const iterator& other) {
                point = other.point;
                offset = other.offset;
                place = other.place;
            }
            /// Return a new iterator which points to the n-next elements
            iterator operator++(int) {
                iterator itr = *this;
                if(*this == point -> end()) {
                    return *this;
                }
                Leaf leaf;
                point->readFile(&leaf, offset, 1, leaf_size);
                if(place == leaf.num - 1) {
                    if(leaf.next == 0) ++place;
                    else {
                        offset = leaf.next;
                        place = 0;
                    }
                }
                else ++place;
                return itr;
            }
            iterator& operator++() {
                if(*this == point->end()){
                    return *this;
                }
                Leaf leaf;
                point->readFile(&leaf, offset, 1, leaf_size);
                if(place == leaf.num - 1){
                    if(leaf.next == 0) ++place;
                    else {
                        offset = leaf.next;
                        place = 0;
                    }
                }
                else ++place;
                return *this;
            }
            iterator operator--(int) {
                iterator itr = *this;
                if(*this == point -> begin()) {
                    return *this;
                }
                Leaf leaf;
                point->readFile(&leaf, offset, 1, leaf_size);
                if(place == 0) {
                    offset = leaf.previous;
                    point->readFile(&leaf, offset, 1, leaf_size);
                    place = leaf.num - 1;
                }
                else -- place;
                return itr;
            }
            iterator& operator--() {
                if(*this == point->begin())   return *this;
                Leaf leaf;
                point -> readFile(&leaf, offset, 1, leaf_size);
                if(place == 0) {
                    offset = leaf.previous;
                    point->readFile(&leaf, offset, 1, leaf_size);
                    place = leaf.num - 1;
                }
                else --place;
                return *this;
            }
            Value getValue() {
                Leaf p;
                point -> readFile(&p, offset, 1, leaf_size);
                return p.data[place].second;
            }
            /// Overloaded of operator '==' and '!='
            /// Check whether the iterators are same
            bool operator==(const iterator& rhs) const {
                return offset == rhs.offset && place == rhs.place && point == rhs.point;
            }
            bool operator==(const const_iterator& rhs) const {
                return offset == rhs.offset && place == rhs.place && point == rhs.point;
            }
            bool operator!=(const iterator& rhs) const {
                return !(*this==rhs);
            }
            bool operator!=(const const_iterator& rhs) const {
                return !(*this==rhs);
            }
        };
        class const_iterator {
            friend class BTree;
        private:
            const BTree *point;
            size_t offset;
            int place;
        public:
            const_iterator() {
                point = nullptr;
                place = offset = 0;
            }
            const_iterator(const const_iterator& other) {
                point = other.point;
                offset = other.offset;
                place = other.place;
            }
            const_iterator(const iterator& other) {
                point = other.point;
                offset = other.offset;
                place = other.place;
            }

            const_iterator(const BTree *bt, size_t off, int pl){
                point = bt;
                offset = off;
                place = pl;
            }
            bool modify(const Value& value){
                Leaf leaf;
                point->readFile((&leaf, offset, 1, leaf_size));
                leaf.data[place].second = value;
                point->writeFile(&leaf, offset, 1, leaf_size);
                return 1;
            }
            // And other methods in iterator, please fill by yourself.
            const_iterator operator++(int) {
                const_iterator itr = *this;
                if(*this == point -> cend()) {
                    return *this;
                }
                Leaf leaf;
                point->readFile(&leaf, offset, 1, leaf_size);
                if(place == leaf.num - 1) {
                    if(leaf.next == 0) ++place;
                    else {
                        offset = leaf.next;
                        place = 0;
                    }
                }
                else ++place;
                return itr;
            }
            const_iterator& operator++() {
                if(*this == point->cend()){
                    return *this;
                }
                Leaf leaf;
                point->readFile(&leaf, offset, 1, leaf_size);
                if(place == leaf.num - 1){
                    if(leaf.next == 0) ++place;
                    else {
                        offset = leaf.next;
                        place = 0;
                    }
                }
                else ++place;
                return *this;
            }
            const_iterator operator--(int) {
                const_iterator itr = *this;
                if(*this == point -> cbegin()) {
                    return *this;
                }
                Leaf leaf;
                point->readFile(&leaf, offset, 1, leaf_size);
                if(place == 0) {
                    offset = leaf.previous;
                    point->readFile(&leaf, offset, 1, leaf_size);
                    place = leaf.num - 1;
                }
                else -- place;
                return itr;
            }
            const_iterator& operator--() {
                const_iterator itr = *this;
                if(*this == point -> cbegin())   return *this;
                Leaf leaf;
                point -> readFile(&leaf, offset, 1, leaf_size);
                if(place == 0) {
                    offset = leaf.previous;
                    point->readFile(&leaf, offset, 1, leaf_size);
                    place = leaf.num - 1;
                }
                else --place;
                return *this;
            }
            Value getValue() {
                Leaf p;
                point -> readFile(&p, offset, 1, leaf_size);
                return p.data[place].second;
            }
            /// Overloaded of operator '==' and '!='
            /// Check whether the iterators are same
            bool operator==(const iterator& rhs) const {
                return offset == rhs.offset && place == rhs.place && point == rhs.point;
            }
            bool operator==(const const_iterator& rhs) const {
                return offset == rhs.offset && place == rhs.place && point == rhs.point;
            }
            bool operator!=(const iterator& rhs) const {
                return !(*this==rhs);
            }
            bool operator!=(const const_iterator& rhs) const {
                return !(*this==rhs);
            }
        };

        /// Default Constructor and Copy Constructor
        BTree() {

            filename.Naming("myBtree");

            fp = NULL;
            openFile();
            if(file_exit == 0)  build_tree();
        }
        BTree(const BTree& other) {
            filename.Naming("myBtree");
            openFile();
            copyFile(filename.str, other.filename.str);
        }
        BTree& operator=(const BTree& other) {
            filename.Naming("myBtree");
            openFile();
            copyFile(filename.str, other.filename.str);
        }
        ~BTree() {
            closeFile();
        }
        // Insert: Insert certain Key-Value into the database
        // Return a pair, the first of the pair is the iterator point to the new
        // element, the second of the pair is Success if it is successfully inserted
        pair<iterator, OperationResult> insert(const Key& key, const Value& value) {
            size_t leaf_offset = locate_leaf(key, tree.root);
            Leaf leaf;
            if(tree.size == 0 || leaf_offset == 0)
            {  ///在空树中增添
                readFile(&leaf, tree.head, 1, leaf_size);
                pair<iterator, OperationResult> result = insert_leaf(leaf, key, value);
                size_t paroffset = leaf.parent;
                Node node;
                while(paroffset != 0){
                    readFile(&node, paroffset, 1, node_size);
                    node.key[0] = key;
                    writeFile(&node, paroffset, 1, node_size);
                    paroffset = node.parent;
                } ///向上设置第一个点的值为新的key直到设置到文件头（根节点）
                //std::cout<<tree.root;
                //size_t tmp = locate_leaf(key, tree.root);
                //std::cout<<"    insert "<<key<<" in "<<tmp<<"\n";
                return result;
            }
            readFile(&leaf, leaf_offset, 1, leaf_size);
            pair<iterator, OperationResult> result = insert_leaf(leaf, key, value);
            //size_t tmp = locate_leaf(key, tree.root);
            //std::cout<<"  insert "<<key<<" in "<<tmp<<"\n";
            return result;

        }
        // Erase: Erase the Key-Value
        // Return Success if it is successfully erased
        // Return Fail if the key doesn't exist in the database
        OperationResult erase(const Key& key) {///erase的归并节点太可怕了,just remian here!
            /// TODO erase function
            return Fail;  // If you can't finish erase part, just remaining here.
        }
        /// Return a iterator to the beginning
        iterator begin(){ return iterator(this, tree.head, 0); }
        const_iterator cbegin() const
        {
            const_iterator itr(this, tree.head, 0);
            return itr;
        }

        // Return a iterator to the end(the next element after the last)
        iterator end() {
            Leaf last;
            readFile(&last, tree.rear, 1, leaf_size);
            return iterator(this, tree.rear, last.num);  ///该点没有值
        }
        const_iterator cend() const {
            Leaf last;
            readFile(&last, tree.rear, 1, leaf_size);
            return const_iterator(this, tree.rear, last.num);
        }
        // Check whether this BTree is empty
        bool empty() const { return tree.size == 0; }
        // Return the number of <K,V> pairs
        size_t size() const { return tree.size; }
        // Clear the BTree
        void clear() {
            fp = fopen(filename, "w");
            fclose(fp);
            openFile();
            build_tree();
        }
        /// Return the value refer to the Key(key)
        Value at(const Key& key){
            iterator itr = find(key);
            Leaf leaf;
            readFile(&leaf, itr.offset, 1, leaf_size);
            return leaf.data[itr.place].second;
        }
        /*
         * Returns the number of elements with key
         *   that compares equivalent to the specified argument,
         * The default method of check the equivalence is !(a < b || b > a)
         */
        size_t count(const Key& key) const {
            if(find(key) == iterator(nullptr))  return 0;
            return 1;
        }
        /*
         * Finds an element with key equivalent to key.
         * key value of the element to search for.
         * Iterator to an element with key equivalent to key.
         *   If no such element is found, past-the-end (see end()) iterator is returned.
         */
        iterator find(const Key& key) {
            Leaf leaf;
            size_t offset = locate_leaf(key, tree.root);
            readFile(&leaf, offset, 1, leaf_size);
            for(int i = 0; i < leaf.num; ++i){
                if(leaf.data[i].first == key)  return iterator(this, offset, i);
            }
            std::cout<<"end\n";
            return end();
        }
        const_iterator find(const Key& key) const {
            Leaf leaf;
            size_t offset = locate_leaf(key, tree.root);
            readFile(&leaf, offset, 1, leaf_size);
            for(int i = 0; i < leaf.num; ++i){
                if(leaf.data[i].first == key)  return const_iterator(this, offset, i);
            }
            return cend();
        }
    };
}  // namespace sjtu

