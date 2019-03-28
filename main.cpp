#include <iostream>
#include <exception>
#include <string>
#include <sstream>
#include <map>
#include <fstream>
#include <memory>
#include <string>

using namespace std;


class TableNode;

enum HeaderType 
{
    RowType,
    ColumnType
};


template<HeaderType T>
class Header
{
public:
    int id;

    // Table line head
    shared_ptr<TableNode> head;

    // Nodes count in line
    int nodes_count = 0;

    // Headers list pointers
    weak_ptr<Header<T>> prev;
    shared_ptr<Header<T>> next;

    Header(int id)
        : id(id) 
    {

    }

    Header(int id, shared_ptr<Header<T>> prev, shared_ptr<Header<T>> next)
        : id(id), prev(prev), next(next) 
    {

    }
};


template<HeaderType T>
class HeaderList
{
private:
    map<int, weak_ptr<Header<T>>> cache;

    void addToCache(shared_ptr<Header<T>> item)
    {
        cache[item->id] = item;
    }

public:
    int length;
    shared_ptr<Header<T>> head;

    HeaderList(int length)
        : length(length)
    {
        if (length <= 0)
        {
            stringstream stream;
            stream << "Wrong length: " << length;
            throw runtime_error(stream.str());
        }

        head = make_shared<Header<T>>(0);
        addToCache(head);

        shared_ptr<Header<T>> prev = head;
        for (int i = 1; i < length; ++i)
        {
            shared_ptr<Header<T>> current = make_shared<Header<T>>(i, prev, head);
            prev->next = current;

            addToCache(current);
            prev = current;
        }
        head->prev = prev;
    }

    shared_ptr<Header<T>> get(int id)
    {
        auto it = cache.find(id);
        if (it != cache.end())
            return it->second.lock();

        return nullptr;
    }
};


using RowHeader = Header<RowType>;
using ColumnHeader = Header<ColumnType>;


class TableNode
{
private:
    weak_ptr<TableNode> _left;
    shared_ptr<TableNode> _right;

    weak_ptr<TableNode> _up;
    shared_ptr<TableNode> _down;

    weak_ptr<RowHeader> _row;
    weak_ptr<ColumnHeader> _column;

public:
    shared_ptr<TableNode> left() const { return _left.lock(); }
    void left(shared_ptr<TableNode> node) { _left = node; }

    shared_ptr<TableNode> right() const { return _right; }
    void right(shared_ptr<TableNode> node) { _right = node; }

    shared_ptr<TableNode> up() const { return _up.lock(); }
    void up(shared_ptr<TableNode> node) { _up = node; }

    shared_ptr<TableNode> down() const { return _down; }
    void down(shared_ptr<TableNode> node) { _down = node; }

    shared_ptr<RowHeader> row() const { return _row.lock(); }
    void row(shared_ptr<RowHeader> node) { _row = node; }

    shared_ptr<ColumnHeader> column() const { return _column.lock(); }
    void column(shared_ptr<ColumnHeader> node) { _column = node; }

    string getDebugRepr()
    {
        stringstream stream;
        stream << "Node (" << this->row()->id << "; " << this->column()->id << "): " << 
                        "LEFT=(" << this->left()->row()->id << "; " << this->left()->column()->id << ") " <<
                        "RIGHT=(" << this->right()->row()->id << "; " << this->right()->column()->id << ") " <<
                        "UP=(" << this->up()->row()->id << "; " << this->up()->column()->id << ") " <<
                        "DOWN=(" << this->down()->row()->id << "; " << this->down()->column()->id << ")" << endl;
        return stream.str();
    }
};


class SparseTable
{
public:
    HeaderList<RowType> rows;
    HeaderList<ColumnType> columns;

    SparseTable(int rows_count, int columns_count)
        : rows(rows_count), columns(columns_count)
    {
        
    }

    shared_ptr<TableNode> create_node(int row_id, int column_id)
    {
        if (row_id < 0 || row_id >= rows.length || column_id < 0 || column_id >= columns.length)
        {
            stringstream stream;
            stream << "Wrong location got: (" << row_id << "; " << column_id << ") for matrix size (" << rows.length << "; " << columns.length << ")";
            throw runtime_error(stream.str());
        }

        shared_ptr<TableNode> node = make_shared<TableNode>();

        auto row = rows.get(row_id);
        node->row(row);
        row->nodes_count++;

        auto column = columns.get(column_id);
        node->column(column);
        column->nodes_count++;

        // Insert into row
        if (!row->head)
        {
            row->head = node;
            node->left(node);
            node->right(node);
        }
        else if (row->head->column()->id > column_id)
        {
            // Need to move head to right
            node->right(row->head);
            node->left(row->head->left());
            node->right()->left(node);
            node->left()->right(node);

            row->head = node;
        }
        else
        {
            shared_ptr<TableNode> p = row->head;
            while (p->right() != row->head && p->right()->column()->id < column_id)
                p = p->right();

            // Check that node is not present yet
            if (p->right()->column()->id == column_id)
            {
                stringstream stream;
                stream << "Node at (" << row_id << "; " << column_id << ") already exists";
                throw runtime_error(stream.str());
            }

            node->left(p);
            node->right(p->right());
            node->left()->right(node);
            node->right()->left(node);
        }

        // Insert to column
        if (!column->head)
        {
            column->head = node;
            node->up(node);
            node->down(node);
        }
        else if (column->head->row()->id > row_id)
        {
            // Need to move head to down
            node->down(column->head);
            node->up(column->head->up());
            node->down()->up(node);
            node->up()->down(node);

            column->head = node;
        }
        else
        {
            shared_ptr<TableNode> p = column->head;
            while (p->down() != column->head && p->down()->row()->id < row_id)
                p = p->down();

            // Check that node is not present yet
            if (p->down()->row()->id == row_id)
            {
                stringstream stream;
                stream << "Node at (" << row_id << "; " << column_id << ") already exists";
                throw runtime_error(stream.str());
            }

            node->up(p);
            node->down(p->down());
            node->up()->down(node);
            node->down()->up(node);
        }

        return node;
    }

    /*
    * Save matrix to file. This is for debug purposes only
    * To be honest, looks quite ugly
    */
    void print_to_file(string filename)
    {
        ofstream fp(filename, ofstream::out);

        // General information first
        fp << "Matrix size: (" << rows.length << "; " << columns.length << ")" << endl;

        fp << "--------------------" << endl;

        // Rows general information
        auto rp = rows.head;
        do
        {
            fp << "Row " << rp->id << " has " << rp->nodes_count << " nodes" << endl;
            rp = rp->next;
        } while (rp != rows.head);

        fp << "--------------------" << endl;

        // Columns general information
        auto cp = columns.head;
        do
        {
            fp << "Column " << cp->id << " has " << cp->nodes_count << " nodes" << endl;
            cp = cp->next;
        } while (cp != columns.head);

        fp << "--------------------" << endl;

        // Detailed nodes dump by rows
        rp = rows.head;
        do
        {
            fp << "Row " << rp->id << " nodes:" << endl;
            
            auto np = rp->head;
            if (np)
            {
                do
                {
                    fp << np->getDebugRepr();
                    np = np->right();
                } while (np != rp->head);
            }

            rp = rp->next;
        } while (rp != rows.head);

        fp << "--------------------" << endl;

        // Detailed nodes dump by columns
        cp = columns.head;
        do
        {
            fp << "Column " << cp->id << " nodes:" << endl;
            
            auto np = cp->head;
            if (np)
            {
                do
                {
                    fp << np->getDebugRepr();
                    np = np->down();
                } while (np != cp->head);
            }

            cp = cp->next;
        } while (cp != columns.head);

        fp.close();
    }
};


void test_matrix_2()
{
    SparseTable matrix(6, 7);

    matrix.create_node(1, 0);
    matrix.create_node(1, 3);

    matrix.create_node(0, 6);
    matrix.create_node(0, 3);
    matrix.create_node(0, 0);

    matrix.create_node(2, 3);
    matrix.create_node(2, 6);
    matrix.create_node(2, 4);

    matrix.create_node(3, 2);
    matrix.create_node(3, 4);
    matrix.create_node(3, 5);

    matrix.create_node(4, 1);
    matrix.create_node(4, 2);
    matrix.create_node(4, 5);
    matrix.create_node(4, 6);

    matrix.create_node(5, 1);
    matrix.create_node(5, 6);

    matrix.print_to_file("test_matrix_2.txt");
}

int main()
{
    test_matrix_2();
//    test_matrix();

    // Filling in the data

    cout << "Hello world!" << endl;
    return 0;
}
