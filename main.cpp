#include <iostream>
#include <exception>
#include <string>
#include <sstream>
#include <map>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <stack>
#include <chrono>


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
    int nodesCount = 0;

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
            if (this->column()->nodesCount > 1)
                this->column()->head(this->down());
            else
                this->column()->head(nullptr);
        }

        this->column()->nodesCount--;
    }

    void restoreInColumn()
    {
        this->down()->up(this->shared_from_this());
        this->up()->down(this->shared_from_this());

        // Update head if needed
        if (!this->column()->head() || this->column()->head()->row()->id > this->row()->id)
            this->column()->head(this->shared_from_this());

        this->column()->nodesCount++;
    }

    void removeFromRow()
    {
        this->right()->left(this->left());
        this->left()->right(this->right());

        // Update head if needed
        if (this->row()->head().get() == this)
        {
            if (this->row()->nodesCount > 1)
                this->row()->head(this->right());
            else
                this->row()->head(nullptr);
        }

        this->row()->nodesCount--;
    }

    void restoreInRow()
    {
        this->right()->left(this->shared_from_this());
        this->left()->right(this->shared_from_this());

        // Update head if needed
        if (!this->row()->head() || this->row()->head()->column()->id > this->column()->id)
            this->row()->head(this->shared_from_this());

        this->row()->nodesCount++;
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
        row->nodesCount++;

        auto column = columns.get(column_id);
        node->column(column);
        column->nodesCount++;

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
            fp << "Row " << rp->id << " has " << rp->nodesCount << " nodes" << endl;
        } while ((rp = rp->next()) != rows.head());

        fp << "--------------------" << endl;

        // Columns general information
        auto cp = columns.head();
        do
        {
            fp << "Column " << cp->id << " has " << cp->nodesCount << " nodes" << endl;
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

struct BackupFrame
{
    shared_ptr<ColumnHeader> column;
    stack<shared_ptr<RowHeader>> rows;
};

class AlgorithmX
{
private:
    SparseTable table;

    bool finished = false;

    shared_ptr<ColumnHeader> findPivotColumn()
    {
        shared_ptr<ColumnHeader> p = table.columns.head();
        shared_ptr<ColumnHeader> pivot = table.columns.head();

        do
        {
            if (p->nodesCount < pivot->nodesCount)
                pivot = p;
        } while ((p = p->next()) != table.columns.head());

        return pivot;
    }

    bool solveIteration(vector<int>& solution)
    {
        if (table.columns.length() == 0)
        {
            // We have solution

            finalSolution = solution;

            return true;
        }

        shared_ptr<ColumnHeader> pivotColumn = findPivotColumn();
        if (pivotColumn->nodesCount == 0)
            return false;

        shared_ptr<RowHeader> pivotRow = pivotColumn->head()->row();
        do
        {
            // Preparations 
            table.ejectRow(pivotRow->id);
            stack<BackupFrame> backup;

            shared_ptr<TableNode> node = pivotRow->head();
            do
            {
                BackupFrame frame;
                shared_ptr<TableNode> p = node->column()->head();
                while (node->column()->nodesCount != 0)
                {
                    frame.rows.push(table.ejectRow(p->row()->id));
                    p = p->down();
                }

                frame.column = table.ejectColumn(node->column()->id);
                backup.push(frame);


            } while ((node = node->right()) != pivotRow->head());

            solution.push_back(pivotRow->id);

            bool done = solveIteration(solution);

            solution.pop_back();

            // Restore ejected
            while (!backup.empty())
            {
                BackupFrame frame = backup.top();

                table.restoreColumn(frame.column);
                while (!frame.rows.empty())
                {
                    table.restoreRow(frame.rows.top());
                    frame.rows.pop();
                }

                backup.pop();
            }

            table.restoreRow(pivotRow);

            if (done)
                return true;

        } while ((pivotRow = pivotRow->next()) != pivotColumn->head()->row());

        return false;
    }

public:
    vector<int> finalSolution;

    AlgorithmX(int setsCount, int universeSize)
        : table(setsCount, universeSize)
    {
    }

    void createNode(int setID, int id)
    {
        table.createNode(setID, id);
    }

    // Return true if solution found
    bool solve()
    {
        if (finished)
            throw runtime_error("Cannot run algorithm twice");

        vector<int> solution;
        solveIteration(solution);

        // Prevent from double execution
        finished = true;

        return finalSolution.size() != 0;
    }
};


class SudokuProblem
{
private:
    static const int PROBLEM_SIZE = 9;

    static const int ROW_COL_OFFSET = 0;
    static const int ROW_NUM_OFFSET = PROBLEM_SIZE * PROBLEM_SIZE;
    static const int COL_NUM_OFFSET = 2 * PROBLEM_SIZE * PROBLEM_SIZE;
    static const int BOX_NUM_OFFSET = 3 * PROBLEM_SIZE * PROBLEM_SIZE;
    static const int FILLED_NUM_OFFSET = 4 * PROBLEM_SIZE * PROBLEM_SIZE;

    int filledCellsCount = 0;
    vector<vector<int>> problem;

public:
    bool hasSolution = false;
    vector<vector<int>> solvedProblem;

    SudokuProblem(int* data)
        : problem(PROBLEM_SIZE, vector<int>(PROBLEM_SIZE, 0)), solvedProblem(PROBLEM_SIZE, vector<int>(PROBLEM_SIZE, 0))
    {
        for (int i = 0; i < PROBLEM_SIZE; ++i)
            for (int j = 0; j < PROBLEM_SIZE; ++j)
            {
                problem[i][j] = data[i + j * PROBLEM_SIZE];
                if (problem[i][j] != 0)
                    ++filledCellsCount;
            }
    }

    // Load sudoku table from text file
    SudokuProblem(char* filename)
        : problem(PROBLEM_SIZE, vector<int>(PROBLEM_SIZE, 0)), solvedProblem(PROBLEM_SIZE, vector<int>(PROBLEM_SIZE, 0))
    {
        ifstream fp(filename, ofstream::in);

        for (int i = 0; i < PROBLEM_SIZE; ++i)
            for (int j = 0; j < PROBLEM_SIZE; ++j)
            {
                int t;
                fp >> t;

                if (t < 0 || t > PROBLEM_SIZE)
                {
                    stringstream stream;
                    stream << "Wrong item in input file: " << t;
                    throw runtime_error(stream.str());
                }

                problem[i][j] = t;

                if (t != 0)
                    ++filledCellsCount;
            }

        fp.close();
    }

    void solve()
    {
        int variants = PROBLEM_SIZE * PROBLEM_SIZE * PROBLEM_SIZE;
        int universePower = 4 * PROBLEM_SIZE * PROBLEM_SIZE + filledCellsCount;
        AlgorithmX algo(variants, universePower);

        // Preparations
        // Row-Column constraints first
        for (int i = 0; i < PROBLEM_SIZE; ++i)
            for (int j = 0; j < PROBLEM_SIZE; ++j)
                for (int v = 0; v < PROBLEM_SIZE; ++v)
                    algo.createNode(packRowID(i, j, v), ROW_COL_OFFSET + packColID(i, j));

        // Row-Number constraints
        for (int i = 0; i < PROBLEM_SIZE; ++i)
            for (int j = 0; j < PROBLEM_SIZE; ++j)
                for (int v = 0; v < PROBLEM_SIZE; ++v)
                    algo.createNode(packRowID(i, j, v), ROW_NUM_OFFSET + packColID(i, v));

        // Column-Number constraints
        for (int i = 0; i < PROBLEM_SIZE; ++i)
            for (int j = 0; j < PROBLEM_SIZE; ++j)
                for (int v = 0; v < PROBLEM_SIZE; ++v)
                    algo.createNode(packRowID(i, j, v), COL_NUM_OFFSET + packColID(j, v));

        // Box-Number constraints
        for (int i = 0; i < PROBLEM_SIZE; ++i)
            for (int j = 0; j < PROBLEM_SIZE; ++j)
                for (int v = 0; v < PROBLEM_SIZE; ++v)
                    algo.createNode(packRowID(i, j, v), BOX_NUM_OFFSET + packColID(getBoxID(i, j), v));

        // Filled numbers constraints
        int counter = 0;
        for (int i = 0; i < PROBLEM_SIZE; ++i)
            for (int j = 0; j < PROBLEM_SIZE; ++j)
            {
                if (problem[i][j] != 0)
                    algo.createNode(packRowID(i, j, problem[i][j] - 1), FILLED_NUM_OFFSET + counter++);
            }

        auto start = std::chrono::steady_clock::now();

        bool success = algo.solve();

        cout << "Solution was successfull: " << success << endl;

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
        double ms = duration.count();

        cout << "Solution took " << ms << "ms" << endl;

        // Decoding result
        hasSolution = success && algo.finalSolution.size() == PROBLEM_SIZE * PROBLEM_SIZE;
        if (hasSolution)
        {
            for (int i = 0; i < algo.finalSolution.size(); ++i)
            {
                int t = algo.finalSolution[i];
                int x = t / (PROBLEM_SIZE * PROBLEM_SIZE);
                int y = (t - x * (PROBLEM_SIZE * PROBLEM_SIZE)) / PROBLEM_SIZE;
                int v = t % PROBLEM_SIZE + 1;

                solvedProblem[x][y] = v;
            }
        }
    }

    int packRowID(int i, int j, int v)
    {
        return i * PROBLEM_SIZE * PROBLEM_SIZE + j * PROBLEM_SIZE + v;
    }

    int packColID(int i, int j)
    {
        return i * PROBLEM_SIZE + j;
    }

    int getBoxID(int i, int j)
    {
        return (j / 3) * 3 + (i / 3);
    }
};

int solve(int* data, int* out)
{
    SudokuProblem p(data);

    try
    {
        p.solve();
    }
    catch (...)
    {
        return -1;
    }

    if (p.hasSolution)
    {
        for (int i = 0; i < 9; ++i)
            for (int j = 0; j < 9; ++j)
                out[i + j * 9] = p.solvedProblem[i][j];

        return 0;
    }
    else
        return -2;
}

int main()
{
    SudokuProblem p("test_sudoku.txt");
    p.solve();

    if (p.hasSolution)
    {
        for (int i = 0; i < 9; i++)
        {
            for (int j = 0; j < 9; j++)
                cout << p.solvedProblem[i][j] << " ";

            cout << endl;
        }
    }

    cin.get();

    return 0;
}
