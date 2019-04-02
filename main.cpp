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
private:
    // Header's list pointers
    weak_ptr<Header<T>> _prev;
    shared_ptr<Header<T>> _next;

    // Table line head
    shared_ptr<TableNode> _head;

public:
    int id;

    shared_ptr<TableNode> head() { return _head; };
    void head(shared_ptr<TableNode> node) { _head = node; };

    // Nodes count in line
    int nodes_count = 0;

    shared_ptr<Header<T>> prev() { return _prev.lock(); };
    void prev(shared_ptr<Header<T>> node) { _prev = node; };

    shared_ptr<Header<T>> next() { return _next; };
    void next(shared_ptr<Header<T>> node) { _next = node; };

    Header(int id)
        : id(id)
    {

    }

    Header(int id, shared_ptr<Header<T>> prev, shared_ptr<Header<T>> next)
        : id(id), _prev(prev), _next(next)
    {

    }
};


template<HeaderType T>
class HeaderList
{
private:
    int _length;

    shared_ptr<Header<T>> _head;

    map<int, weak_ptr<Header<T>>> cache;

    void addToCache(shared_ptr<Header<T>> item)
    {
        cache[item->id] = item;
    }

public:
    int length() { return _length; };

    shared_ptr<Header<T>> head() { return _head; };

    HeaderList(int length)
        : _length(length)
    {
        if (length <= 0)
        {
            stringstream stream;
            stream << "Wrong length: " << length;
            throw runtime_error(stream.str());
        }

        _head = make_shared<Header<T>>(0);
        addToCache(_head);

        shared_ptr<Header<T>> p = _head;
        for (int i = 1; i < length; ++i)
        {
            shared_ptr<Header<T>> current = make_shared<Header<T>>(i, p, _head);
            p->next(current);

            addToCache(current);
            p = current;
        }
        _head->prev(p);
    }

    shared_ptr<Header<T>> get(int id)
    {
        auto it = cache.find(id);
        if (it != cache.end())
            return it->second.lock();

        return nullptr;
    }

    shared_ptr<Header<T>> eject(int id)
    {
        auto it = cache.find(id);
        if (it != cache.end())
        {
            auto header = it->second.lock();

            // Remove from list
            header->prev()->next(header->next());
            header->next()->prev(header->prev());

            // Remove from cache
            cache.erase(it);

            // Adjust head if needed
            if (_head == header)
                _head = _length > 1 ? _head->next() : nullptr;

            _length--;

            return header;
        }

        return nullptr;
    }

    void restore(shared_ptr<Header<T>> header)
    {
        // Restore in list
        header->prev()->next(header);
        header->next()->prev(header);

        // Restore in cache
        addToCache(header);

        // Adjust head if needed
        if (!_head || header->id < _head->id)
            _head = header;

        _length++;
    }
};


using RowHeader = Header<RowType>;
using ColumnHeader = Header<ColumnType>;


class TableNode : public enable_shared_from_this<TableNode>
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

    // Insert this horizontally after node
    void insertAfterH(shared_ptr<TableNode> node)
    {
        this->left(node);
        this->right(node->right());
        this->left()->right(this->shared_from_this());
        this->right()->left(this->shared_from_this());
    }

    // Insert this vertically after node
    void insertAfterV(shared_ptr<TableNode> node)
    {
        this->up(node);
        this->down(node->down());
        this->up()->down(this->shared_from_this());
        this->down()->up(this->shared_from_this());
    }

    void removeFromColumn()
    {
        this->down()->up(this->up());
        this->up()->down(this->down());

        // Update head if needed
        if (this->column()->head().get() == this)
        {
            if (this->column()->nodes_count > 1)
                this->column()->head(this->down());
            else
                this->column()->head(nullptr);
        }

        this->column()->nodes_count--;
    }

    void restoreInColumn()
    {
        this->down()->up(this->shared_from_this());
        this->up()->down(this->shared_from_this());

        // Update head if needed
        if (this->column()->head()->row()->id > this->row()->id)
            this->column()->head(this->shared_from_this());

        this->column()->nodes_count++;
    }

    void removeFromRow()
    {
        this->right()->left(this->left());
        this->left()->right(this->right());

        // Update head if needed
        if (this->row()->head().get() == this)
        {
            if (this->row()->nodes_count > 1)
                this->row()->head(this->right());
            else
                this->row()->head(nullptr);
        }

        this->row()->nodes_count--;
    }

    void restoreInRow()
    {
        this->right()->left(this->shared_from_this());
        this->left()->right(this->shared_from_this());

        // Update head if needed
        if (this->row()->head()->column()->id > this->column()->id)
            this->row()->head(this->shared_from_this());

        this->row()->nodes_count++;
    }

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

    shared_ptr<TableNode> createNode(int row_id, int column_id)
    {
        if (row_id < 0 || row_id >= rows.length() || column_id < 0 || column_id >= columns.length())
        {
            stringstream stream;
            stream << "Wrong location got: (" << row_id << "; " << column_id << ") for matrix size (" << rows.length() << "; " << columns.length() << ")";
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
        if (!row->head())
        {
            row->head(node);
            node->left(node);
            node->right(node);
        }
        else if (row->head()->column()->id > column_id)
        {
            // Need to move head to right
            node->insertAfterH(row->head()->left());
            row->head(node);
        }
        else
        {
            shared_ptr<TableNode> p = row->head();
            while (p->right() != row->head() && p->right()->column()->id < column_id)
                p = p->right();

            // Check that node is not present yet
            if (p->right()->column()->id == column_id)
            {
                stringstream stream;
                stream << "Node at (" << row_id << "; " << column_id << ") already exists";
                throw runtime_error(stream.str());
            }

            node->insertAfterH(p);
        }

        // Insert to column
        if (!column->head())
        {
            column->head(node);
            node->up(node);
            node->down(node);
        }
        else if (column->head()->row()->id > row_id)
        {
            // Need to move head to down
            node->insertAfterV(column->head()->up());
            column->head(node);
        }
        else
        {
            shared_ptr<TableNode> p = column->head();
            while (p->down() != column->head() && p->down()->row()->id < row_id)
                p = p->down();

            // Check that node is not present yet
            if (p->down()->row()->id == row_id)
            {
                stringstream stream;
                stream << "Node at (" << row_id << "; " << column_id << ") already exists";
                throw runtime_error(stream.str());
            }

            node->insertAfterV(p);
        }

        return node;
    }

    shared_ptr<ColumnHeader> ejectColumn(int id)
    {
        shared_ptr<ColumnHeader> column = columns.eject(id);

        auto p = column->head();
        if (p)
        {
            do
            {
                p->removeFromRow();
            } while ((p = p->down()) != column->head());
        }

        return column;
    }

    void restoreColumn(shared_ptr<ColumnHeader> column)
    {
        columns.restore(column);

        auto p = column->head();
        if (p)
        {
            do
            {
                p->restoreInRow();
            } while ((p = p->down()) != column->head());
        }
    }

    shared_ptr<RowHeader> ejectRow(int id)
    {
        shared_ptr<RowHeader> row = rows.eject(id);

        auto p = row->head();
        if (p)
        {
            do
            {
                p->removeFromColumn();
            } while ((p = p->right()) != row->head());
        }

        return row;
    }

    void restoreRow(shared_ptr<RowHeader> row)
    {
        rows.restore(row);

        auto p = row->head();
        if (p)
        {
            do
            {
                p->restoreInColumn();
            } while ((p = p->right()) != row->head());
        }
    }

    /*
    * Save matrix to file. This is for debug purposes only
    * To be honest, looks quite ugly
    */
    void printToFile(string filename)
    {
        ofstream fp(filename, ofstream::out);

        // General information first
        fp << "Matrix size: (" << rows.length() << "; " << columns.length() << ")" << endl;

        fp << "--------------------" << endl;

        // Rows general information
        auto rp = rows.head();
        do
        {
            fp << "Row " << rp->id << " has " << rp->nodes_count << " nodes" << endl;
        } while ((rp = rp->next()) != rows.head());

        fp << "--------------------" << endl;

        // Columns general information
        auto cp = columns.head();
        do
        {
            fp << "Column " << cp->id << " has " << cp->nodes_count << " nodes" << endl;
        } while ((cp = cp->next()) != columns.head());

        fp << "--------------------" << endl;

        // Detailed nodes dump by rows
        rp = rows.head();
        do
        {
            fp << "Row " << rp->id << " nodes:" << endl;

            auto np = rp->head();
            if (np)
            {
                do
                {
                    fp << np->getDebugRepr();
                } while ((np = np->right()) != rp->head());
            }
        } while ((rp = rp->next()) != rows.head());

        fp << "--------------------" << endl;

        // Detailed nodes dump by columns
        cp = columns.head();
        do
        {
            fp << "Column " << cp->id << " nodes:" << endl;

            auto np = cp->head();
            if (np)
            {
                do
                {
                    fp << np->getDebugRepr();
                } while ((np = np->down()) != cp->head());
            }
        } while ((cp = cp->next()) != columns.head());

        fp.close();
    }
};


void testMatrix2()
{
    SparseTable matrix(6, 7);

    matrix.createNode(1, 0);
    matrix.createNode(1, 3);

    matrix.createNode(0, 6);
    matrix.createNode(0, 3);
    matrix.createNode(0, 0);

    matrix.createNode(2, 3);
    matrix.createNode(2, 6);
    matrix.createNode(2, 4);

    matrix.createNode(3, 2);
    matrix.createNode(3, 4);
    matrix.createNode(3, 5);

    matrix.createNode(4, 1);
    matrix.createNode(4, 2);
    matrix.createNode(4, 5);
    matrix.createNode(4, 6);

    matrix.createNode(5, 1);
    matrix.createNode(5, 6);

    matrix.printToFile("test_matrix_3.txt");

    auto row = matrix.ejectRow(0);
    auto column = matrix.ejectColumn(0);

    matrix.printToFile("test_matrix_4.txt");

    matrix.restoreColumn(column);
    matrix.restoreRow(row);

    matrix.printToFile("test_matrix_5.txt");
}

int main()
{
    testMatrix2();

    cout << "Hello world!" << endl;

    return 0;
}
