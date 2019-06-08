# include "utility.hpp"
# include <fstream>
# include <functional>
# include <cstddef>
# include "exception.hpp"

namespace sjtu {

	int ID = 0;
	template <class KeyType, class ValueType, class Compare = std::less<KeyType> >
	class BTree {
		public:
			typedef pair <KeyType, ValueType> value_type;

			class iterator;

			class const_iterator;

		private:
			static const int M = (4079 / (sizeof(size_t) + sizeof(KeyType))) < 5 ? 4 : (4079 / (sizeof(size_t) + sizeof(KeyType)) - 1);                // need modify
			static const int L = (4076 / (sizeof(value_type))) < 5 ? 4 : (4076 / (sizeof(value_type)) - 1);                 // need modify
//			static const int M = 1000;
//			static const int L = 200;
			static const int MMIN = (M+1) / 2;            // M / 2
			static const int LMIN = (L+1) / 2;            // L / 2
			static const int tree_offset = 0;

			struct nameString {
				char *str;

				nameString() { str = new char[10]; }

				~nameString() { if (str != nullptr) delete str; }

				void setName(int id) {
					if (id < 0 || id > 9) throw "no more B plus Tree!";
					str[0] = 'd';
					str[1] = 'a';
					str[2] = 't';
					str[3] = static_cast <char> (id + '0');
					str[4] = '.';
					str[5] = 'd';
					str[6] = 'a';
					str[7] = 't';
					str[8] = '\0';
				}

				void setName(char *_str) {
					int i = 0;
					for (; _str[i]; ++i) str[i] = _str[i];
					str[i] = 0;
				}
			};

			struct TREE {
				size_t head;          // head of leaf
				size_t tail;          // tail of leaf
				size_t root;          // root of Btree
				size_t size;          // size of Btree
				size_t eof;         // end of file
				TREE() {
					head = 0;
					tail = 0;
					root = 0;
					size = 0;
					eof = 0;
				}
			};

			struct Leaf {
				size_t offset;          // offset
				size_t par;               // parent
				size_t pre, next;          // previous and next leaf
				int num;                  // number of pairs in leaf
				value_type data[L + 1];   // data
				Leaf() {
					offset = 0, par = 0, pre = 0, next = 0, num = 0;
				}
			};
			struct Node {
				size_t offset;      	// offset
				size_t par;           	// parent
				size_t ch[M + 1];     	// children
				KeyType key[M + 1];   	// key
				int num;              	// number in internal node
				bool type;            	// child is leaf or not
				Node() {
					offset = 0, par = 0;
					for (int i = 0; i <= M; ++i) ch[i] = 0;
					num = 0;
					type = 0;
				}
			};


			FILE *fp;
			bool fp_open;
			nameString fp_name;
			TREE tree;

			// ================================= file operation ===================================== //
			/**
			 * Instructions:
			 *    openFile(): if file exists, open it; else create it and open it.
			 *    closeFile(): close file.
			 *    readFile(*place, offset, num, size): read size * num bytes from the offset position of the
			 *                                         file, then put it in *place. return successful operations.
			 *    writeFile(*place, offset, num, size): write size * num bytes to the offset position of the
			 *                                         file from *place, return successful operations.
			 *    copy_readFile(*place, offset, num, size): the same function as readFile, just different filename.
			 *    copy_leaf(offset, from_offset, par_offset): copy the leaf from from_offset to offset.
			 *    copy_node(offset, from_offset, par_offset): copy the internal node from from_offset to offset.
			 *    copy_File(name1, name2): copy file from name1 to name2.
			 */
			bool file_already_exists;

			inline void openFile() {
				file_already_exists = 1;
				if (fp_open == 0) {
					fp = fopen(fp_name.str, "rb+");
					if (fp == nullptr) {
						file_already_exists = 0;
						fp = fopen(fp_name.str, "w");
						fclose(fp);
						fp = fopen(fp_name.str, "rb+");
					} else readFile(&tree, tree_offset, 1, sizeof(TREE));
					fp_open = 1;
				}
			}

			inline void closeFile() {
				if (fp_open == 1) {
					fclose(fp);
					fp_open = 0;
				}
			}

			inline void readFile(void *place, size_t offset, size_t num, size_t size) const {
				if (fseek(fp, offset, SEEK_SET)) throw "open file failed!";
				fread(place, size, num, fp);
			}

			inline void writeFile(void *place, size_t offset, size_t num, size_t size) const {
				if (fseek(fp, offset, SEEK_SET)) throw "open file failed!";
				fwrite(place, size, num, fp);
			}

			nameString fp_from_name;
			FILE *fp_from;

			inline void copy_readFile(void *place, size_t offset, size_t num, size_t size) const{
				if (fseek(fp_from, offset, SEEK_SET)) throw "open file failed";
				size_t ret = fread(place, num, size, fp_from);
			}

			size_t leaf_size_temp;

			inline void copy_leaf(size_t offset, size_t from_offset, size_t par_offset) {
				Leaf leaf, leaf_from, pre_leaf;
				copy_readFile(&leaf_from, from_offset, 1, sizeof(Leaf));
				leaf.offset = offset, leaf.par = par_offset;
				leaf.num = leaf_from.num; leaf.pre = leaf_size_temp; leaf.next = 0;
				if(leaf_size_temp != 0) {
					readFile(&pre_leaf, leaf_size_temp, 1, sizeof(Leaf));
					pre_leaf.next = offset;
					writeFile(&pre_leaf, leaf_size_temp, 1, sizeof(Leaf));
					tree.tail = offset;
				} else tree.head = offset;
				for (int i=0; i<leaf.num; ++i) leaf.data[i].first = leaf_from.data[i].first, leaf.data[i].second = leaf_from.data[i].second;
				writeFile(&leaf, offset, 1, sizeof(Leaf));
				tree.eof += sizeof(Leaf);
				leaf_size_temp = offset;
			}

			inline void copy_node(size_t offset, size_t from_offset, size_t par_offset) {
				Node node, node_from;
				copy_readFile(&node_from, from_offset, 1, sizeof(Node));
				writeFile(&node, offset, 1, sizeof(Node));
				tree.eof += sizeof(Node);
				node.offset = offset; node.par = par_offset;
				node.num = node_from.num; node.type = node_from.type;
				for (int i=0; i<node.num; ++i) {
					node.key[i] = node_from.key[i];
					if(node.type == 1) {  					// leaf
						copy_leaf(tree.eof, node_from.ch[i], offset);
					} else {                        // node
						copy_node(tree.eof, node_from.ch[i], offset);
					}
				}
				writeFile(&node, offset, 1, sizeof(Node));
			}

			inline void copyFile(char *to, char *from) {
				fp_from_name.setName(from);
				fp_from = fopen(fp_from_name.str, "rb+");
				if (fp_from == nullptr) throw "no such file";
				TREE treeo;
				copy_readFile(&treeo, tree_offset, 1, sizeof(TREE));
				leaf_size_temp = 0; tree.size = treeo.size;
				tree.root = tree.eof = sizeof(TREE);
				writeFile(&tree, tree_offset, 1, sizeof(TREE));
				copy_node(tree.root, treeo.root, 0);
				writeFile(&tree, tree_offset, 1, sizeof(TREE));
				fclose(fp_from);
			}

			// ============================= end of file operation =================================== //

			/**
			 * function: build an tree with no elements.
			 */
			inline void build_tree() {
				tree.size = 0;
				tree.eof = sizeof(TREE);
				Node root;
				Leaf leaf;
				tree.root = root.offset = tree.eof;
				tree.eof += sizeof(Node);
				tree.head = tree.tail = leaf.offset = tree.eof;
				tree.eof += sizeof(Leaf);
				root.par = 0; root.num = 1; root.type = 1;
				root.ch[0] = leaf.offset;
				leaf.par = root.offset;
				leaf.next = leaf.pre = 0;
				leaf.num = 0;
				writeFile(&tree, tree_offset, 1, sizeof(TREE));
				writeFile(&root, root.offset, 1, sizeof(Node));
				writeFile(&leaf, leaf.offset, 1, sizeof(Leaf));
			}

			/**
			 * function: given a key and find the leaf it should be in.
			 * return the offset of the leaf.
			 */
			size_t locate_leaf(const KeyType &key, size_t offset) const {
				Node p;
				readFile(&p, offset, 1, sizeof(Node));
				if(p.type == 1) {
					// child -> leaf
					int pos = 0;
					for (; pos < p.num; ++pos)
						if (key < p.key[pos]) break;
					if (pos == 0) return 0;
					return p.ch[pos - 1];
				} else {
					int pos = 0;
					for (; pos < p.num; ++pos)
						if (key < p.key[pos]) break;
					if (pos == 0) return 0;
					return locate_leaf(key, p.ch[pos - 1]);
				}
			}

			/**
			 * function: insert an element (key, value) to the given leaf.
			 * return Fail and operate nothing if there are elements with same key.
			 * return Success if inserted.
			 * if leaf count is bigger than L then call split_leaf().
			 */
			pair <iterator, OperationResult> insert_leaf(Leaf &leaf, const KeyType &key, const ValueType &value) {
				iterator ret;
				int pos = 0;
				for (; pos < leaf.num; ++pos) {
					if (key == leaf.data[pos].first) return pair <iterator, OperationResult> (iterator(nullptr), Fail);			// there are elements with the same key
					if (key < leaf.data[pos].first) break;
				}
				for (int i = leaf.num - 1; i >= pos; --i)
					leaf.data[i+1].first = leaf.data[i].first, leaf.data[i+1].second = leaf.data[i].second;
				leaf.data[pos].first = key; leaf.data[pos].second = value;
				++leaf.num;
				++tree.size;
				ret.from = this; ret.place = pos; ret.offset = leaf.offset;
				writeFile(&tree, tree_offset, 1, sizeof(TREE));
				if(leaf.num <= L) writeFile(&leaf, leaf.offset, 1, sizeof(Leaf));
				else split_leaf(leaf, ret, key);
				return pair <iterator, OperationResult> (ret, Success);
			}

			/**
			 * function: insert an key elements (only key) to the given internal node.
			 *           insert an child to the given internal node.
			 * notice: elements in child is bigger than key.
			 * if node count is bigger than M then call split_node().
			 */
			void insert_node(Node &node, const KeyType &key, size_t ch) {
				int pos = 0;
				for (; pos < node.num; ++pos)
					if (key < node.key[pos]) break;
				for (int i = node.num - 1; i >= pos; --i)
					node.key[i+1] = node.key[i];
				for (int i = node.num - 1; i >= pos; --i)
					node.ch[i+1] = node.ch[i];
				node.key[pos] = key;
				node.ch[pos] = ch;
				++node.num;
				if(node.num <= M) writeFile(&node, node.offset, 1, sizeof(Node));
				else split_node(node);
			}

			/**
			 * function: split a leaf into two parts.
			 * then, call insert_node() to insert a (key, ch) pair in the father node.
			 */
			void split_leaf(Leaf &leaf, iterator &it, const KeyType &key) {
				Leaf newleaf;
				newleaf.num = leaf.num - (leaf.num >> 1);
				leaf.num = leaf.num >> 1;
				newleaf.offset = tree.eof;
				tree.eof += sizeof(Leaf);
				newleaf.par = leaf.par;
				for (int i=0; i<newleaf.num; ++i) {
					newleaf.data[i].first = leaf.data[i + leaf.num].first, newleaf.data[i].second = leaf.data[i + leaf.num].second;
					if(newleaf.data[i].first == key) {
						it.offset = newleaf.offset;
						it.place = i;
					}
				}
				// link operation begin
				newleaf.next = leaf.next;
				newleaf.pre = leaf.offset;
				leaf.next = newleaf.offset;
				Leaf nextleaf;
				if(newleaf.next == 0) tree.tail = newleaf.offset;
				else {
					readFile(&nextleaf, newleaf.next, 1, sizeof(Leaf));
					nextleaf.pre = newleaf.offset;
					writeFile(&nextleaf, nextleaf.offset, 1, sizeof(Leaf));
				}
				// link operation end

				writeFile(&leaf, leaf.offset, 1, sizeof(Leaf));
				writeFile(&newleaf, newleaf.offset, 1, sizeof(Leaf));
				writeFile(&tree, tree_offset, 1, sizeof(TREE));

				// update father
				Node par;
				readFile(&par, leaf.par, 1, sizeof(Node));
				insert_node(par, newleaf.data[0].first, newleaf.offset);
			}

			/**
			 * function: split a node into two parts.
			 * then, call insert_node() to insert a (key, ch) pair in the father node.
			 */
			void split_node(Node &node) {
				Node newnode;
				newnode.num = node.num - (node.num >> 1);
				node.num >>= 1;
				newnode.par = node.par;
				newnode.type = node.type;
				newnode.offset = tree.eof;
				tree.eof += sizeof(Node);
				for (int i = 0; i < newnode.num; ++i)
					newnode.key[i] = node.key[i + node.num];
				for (int i = 0; i < newnode.num; ++i)
					newnode.ch[i] = node.ch[i + node.num];

				// updating children's parents
				Leaf leaf;
				Node internal;
				for (int i = 0; i < newnode.num; ++i) {
					if(newnode.type == 1) {  				// his child is leaf
						readFile(&leaf, newnode.ch[i], 1, sizeof(Leaf));
						leaf.par = newnode.offset;
						writeFile(&leaf, leaf.offset, 1, sizeof(Leaf));
					} else {
						readFile(&internal, newnode.ch[i], 1, sizeof(Node));
						internal.par = newnode.offset;
						writeFile(&internal, internal.offset, 1, sizeof(Node));
					}
				}

				if(node.offset == tree.root) {				// root
					// new root
					Node newroot;
					newroot.par = 0;
					newroot.type = 0;
					newroot.offset = tree.eof;
					tree.eof += sizeof(Node);
					newroot.num = 2;
					newroot.key[0] = node.key[0];
					newroot.ch[0] = node.offset;
					newroot.key[1] = newnode.key[0];
					newroot.ch[1] = newnode.offset;
					node.par = newroot.offset;
					newnode.par = newroot.offset;
					tree.root = newroot.offset;

					writeFile(&tree, tree_offset, 1, sizeof(TREE));
					writeFile(&node, node.offset, 1, sizeof(Node));
					writeFile(&newnode, newnode.offset, 1, sizeof(Node));
					writeFile(&newroot, newroot.offset, 1, sizeof(Node));
				} else {															// not root
					writeFile(&tree, tree_offset, 1, sizeof(TREE));
					writeFile(&node, node.offset, 1, sizeof(Node));
					writeFile(&newnode, newnode.offset, 1, sizeof(Node));

					Node par;
					readFile(&par, node.par, 1, sizeof(Node));
					insert_node(par, newnode.key[0], newnode.offset);
				}
			}


		public:
			class iterator {
					friend class BTree;
				private:
					size_t offset;        // offset of the leaf node
					int place;							// place of the element in the leaf node
					BTree *from;
				public:
					iterator() {
						from = nullptr;
						place = 0, offset = 0;
					}
					iterator(BTree *_from, size_t _offset = 0, int _place = 0) {
						from = _from;
						offset = _offset; place = _place;
					}
					iterator(const iterator& other) {
						from = other.from;
						offset = other.offset;
						place = other.place;
					}
					iterator(const const_iterator& other) {
						from = other.from;
						offset = other.offset;
						place = other.place;
					}

					OperationResult modify(const ValueType& value) {
						Leaf p;
						from -> readFile(&p, offset, 1, sizeof(Leaf));
						p.data[place].second = value;
						from -> writeFile(&p, offset, 1, sizeof(Leaf));
						return Success;
					}

					// Return a new iterator which points to the n-next elements
					iterator operator++(int) {
						iterator ret = *this;
						// end of bptree
						if(*this == from -> end()) {
							from = nullptr; place = 0; offset = 0;
							return ret;
						}
						Leaf p;
						from -> readFile(&p, offset, 1, sizeof(Leaf));
						if(place == p.num - 1) {
							if(p.next == 0) ++ place;
							else {
								offset = p.next;
								place = 0;
							}
						} else ++ place;
						return ret;
					}
					iterator& operator++() {
						if(*this == from -> end()) {
							from = nullptr; place = 0; offset = 0;
							return *this;
						}
						Leaf p;
						from -> readFile(&p, offset, 1, sizeof(Leaf));
						if(place == p.num - 1) {
							if(p.next == 0) ++ place;
							else {
								offset = p.next;
								place = 0;
							}
						} else ++ place;
						return *this;
					}
					iterator operator--(int) {
						iterator ret = *this;
						if(*this == from -> begin()) {
							from = nullptr; place = 0; offset = 0;
							return ret;
						}
						Leaf p, q;
						from -> readFile(&p, offset, 1, sizeof(Leaf));
						if(place == 0) {
							offset = p.pre;
							from -> readFile(&q, p.pre, 1, sizeof(Leaf));
							place = q.num - 1;
						} else -- place;
						return ret;
					}
					iterator& operator--() {
						if(*this == from -> begin()) {
							from = nullptr; place = 0; offset = 0;
							return *this;
						}
						Leaf p, q;
						from -> readFile(&p, offset, 1, sizeof(Leaf));
						if(place == 0) {
							offset = p.pre;
							from -> readFile(&q, p.pre, 1, sizeof(Leaf));
							place = q.num - 1;
						} else -- place;
						return *this;
					}
					bool operator==(const iterator& rhs) const {
						return offset == rhs.offset && place == rhs.place && from == rhs.from;
					}
					bool operator==(const const_iterator& rhs) const {
						return offset == rhs.offset && place == rhs.place && from == rhs.from;
					}
					bool operator!=(const iterator& rhs) const {
						return offset != rhs.offset || place != rhs.place || from != rhs.from;
					}
					bool operator!=(const const_iterator& rhs) const {
						return offset != rhs.offset || place != rhs.place || from != rhs.from;
					}
			};

			class const_iterator {
					friend class BTree;
				private:
					size_t offset;        // offset of the leaf node
					int place;							// place of the element in the leaf node
					const BTree *from;
				public:
					const_iterator() {
						from = nullptr;
						place = 0, offset = 0;
					}
					const_iterator(const BTree *_from, size_t _offset = 0, int _place = 0) {
						from = _from;
						offset = _offset; place = _place;
					}
					const_iterator(const iterator& other) {
						from = other.from;
						offset = other.offset;
						place = other.place;
					}
					const_iterator(const const_iterator& other) {
						from = other.from;
						offset = other.offset;
						place = other.place;
					}
					const_iterator operator++(int) {
						const_iterator ret = *this;
						// end of bptree
						if(*this == from -> cend()) {
							from = nullptr; place = 0; offset = 0;
							return ret;
						}
						Leaf p;
						from -> readFile(&p, offset, 1, sizeof(Leaf));
						if(place == p.num - 1) {
							if(p.next == 0) ++ place;
							else {
								offset = p.next;
								place = 0;
							}
						} else ++ place;
						return ret;
					}
					const_iterator& operator++() {
						if(*this == from -> cend()) {
							from = nullptr; place = 0; offset = 0;
							return *this;
						}
						Leaf p;
						from -> readFile(&p, offset, 1, sizeof(Leaf));
						if(place == p.num - 1) {
							if(p.next == 0) ++ place;
							else {
								offset = p.next;
								place = 0;
							}
						} else ++ place;
						return *this;
					}
					const_iterator operator--(int) {
						const_iterator ret = *this;
						if(*this == from -> cbegin()) {
							from = nullptr; place = 0; offset = 0;
							return ret;
						}
						Leaf p, q;
						from -> readFile(&p, offset, 1, sizeof(Leaf));
						if(place == 0) {
							offset = p.pre;
							from -> readFile(&q, p.pre, 1, sizeof(Leaf));
							place = q.num - 1;
						} else -- place;
						return ret;
					}
					const_iterator& operator--() {
						if(*this == from -> cbegin()) {
							from = nullptr; place = 0; offset = 0;
							return *this;
						}
						Leaf p, q;
						from -> readFile(&p, offset, 1, sizeof(Leaf));
						if(place == 0) {
							offset = p.pre;
							from -> readFile(&q, p.pre, 1, sizeof(Leaf));
							place = q.num - 1;
						} else -- place;
						return *this;
					}
					bool operator==(const iterator& rhs) const {
						return offset == rhs.offset && place == rhs.place && from == rhs.from;
					}
					bool operator==(const const_iterator& rhs) const {
						return offset == rhs.offset && place == rhs.place && from == rhs.from;
					}
					bool operator!=(const iterator& rhs) const {
						return offset != rhs.offset || place != rhs.place || from != rhs.from;
					}
					bool operator!=(const const_iterator& rhs) const {
						return offset != rhs.offset || place != rhs.place || from != rhs.from;
					}
			};

			// Default Constructor and Copy Constructor

			BTree() {
				fp_name.setName(ID);
				fp = nullptr;
				openFile();
				if (file_already_exists == 0) build_tree();
			}

			BTree(const BTree& other) {
				fp_name.setName(ID);
				openFile();
				copyFile(fp_name.str, other.fp_name.str);
			}

			BTree& operator=(const BTree& other) {
				fp_name.setName(ID);
				openFile();
				copyFile(fp_name.str, other.fp_name.str);
			}

			~BTree() {
				closeFile();
			}

			/**
			 * Insert: Insert certain Key-Value into the database
			 * Return a pair, the first of the pair is the iterator point to the new
			 * element, the second of the pair is Success if it is successfully inserted
			 */
			pair <iterator, OperationResult> insert(const KeyType& key, const ValueType& value) {
				size_t leaf_offset = locate_leaf(key, tree.root);
				Leaf leaf;
				if(tree.size == 0 || leaf_offset == 0) {					// smallest elements
					readFile(&leaf, tree.head, 1, sizeof(Leaf));
					pair <iterator, OperationResult> ret = insert_leaf(leaf, key, value);
					if(ret.second == Fail) return ret;
					size_t offset = leaf.par;
					Node node;
					while(offset != 0) {
						readFile(&node, offset, 1, sizeof(Node));
						node.key[0] = key;
						writeFile(&node, offset, 1, sizeof(Node));
						offset = node.par;
					}
					return ret;
				}
				readFile(&leaf, leaf_offset, 1, sizeof(Leaf));
				pair <iterator, OperationResult> ret = insert_leaf(leaf, key, value);
				return ret;
			}
              OperationResult erase(const KeyType& key) {
                return Success;
                }


			iterator begin() {
				return iterator(this, tree.head, 0);
			}
			const_iterator cbegin() const {
				return const_iterator(this, tree.head, 0);
			}
			// Return a iterator to the end(the next element after the last)
			iterator end() {
				Leaf tail;
				readFile(&tail, tree.tail, 1, sizeof(Leaf));
				return iterator(this, tree.tail, tail.num);
			}
			const_iterator cend() const {
				Leaf tail;
				readFile(&tail, tree.tail, 1, sizeof(Leaf));
				return const_iterator(this, tree.tail, tail.num);
			}
			// Check whether this BTree is empty
			bool empty() const {return tree.size == 0;}
			// Return the number of <K,V> pairs
			size_t size() const {return tree.size;}
			// Clear the BTree
			void clear() {
				fp = fopen(fp_name.str, "w");
				fclose(fp);
				openFile();
				build_tree();
			}
			/**
			 * Returns the number of elements with key
			 *   that compares equivalent to the specified argument,
			 * The default method of check the equivalence is !(a < b || b > a)
			 */
			size_t count(const KeyType& key) const {
				return static_cast <size_t> (find(key) != cend());
			}
			ValueType at(const KeyType& key){
				iterator it = find(key);
				Leaf leaf;
				if(it == end()) {
					throw "not found";
				}
				readFile(&leaf, it.offset, 1, sizeof(Leaf));
				return leaf.data[it.place].second;
			}
			/**
			 * Finds an element with key equivalent to key.
			 * key value of the element to search for.
			 * Iterator to an element with key equivalent to key.
			 *   If no such element is found, past-the-end (see end()) iterator is
			 * returned.`
			 */
			iterator find(const KeyType& key) {
				size_t leaf_offset = locate_leaf(key, tree.root);
				if(leaf_offset == 0) return end();
				Leaf leaf;
				readFile(&leaf, leaf_offset, 1, sizeof(Leaf));
				for (int i = 0; i < leaf.num; ++i)
					if (leaf.data[i].first == key) return iterator(this, leaf_offset, i);
				return end();
			}
			const_iterator find(const KeyType& key) const {
				size_t leaf_offset = locate_leaf(key, tree.root);
				if(leaf_offset == 0) return cend();
				Leaf leaf;
				readFile(&leaf, leaf_offset, 1, sizeof(Leaf));
				for (int i = 0; i < leaf.num; ++i)
					if (leaf.data[i].first == key) return const_iterator(this, leaf_offset, i);
				return cend();
			}
			
	};
}  // namespace sjtu

