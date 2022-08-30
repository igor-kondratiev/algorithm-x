#include <cassert>
#include <iostream>
#include <string>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <stack>
#include <chrono>
#include <limits>


using namespace std;


class TableNode;

enum HeaderType
{
    RowType,
    ColumnType
};

const uint16_t INVALID_NODE_ID = numeric_limits<uint16_t>::max();


template<HeaderType T>
struct Header
{
    uint16_t m_id;

    uint16_t m_nextId;
    uint16_t m_prevId;

    uint16_t m_nodesCount = 0;
    uint16_t m_headNodeId = INVALID_NODE_ID;

    Header(uint16_t id, uint16_t nextId, uint16_t prevId)
        : m_id(id)
        , m_nextId(nextId)
        , m_prevId(prevId)
    {
    }

    Header(Header&&) = default;
    Header(const Header&) = delete;
    Header& operator=(const Header&) = delete;

    inline bool isEmpty() const { return m_nodesCount == 0; }
};


template<HeaderType T>
struct HeaderList
{
    uint16_t m_headId = 0;
    uint16_t m_length;
    vector<Header<T>> m_nodesPool;

    HeaderList(uint16_t length)
        : m_length(length)
    {
        assert(length > 0);

        m_nodesPool.reserve(length);
        m_nodesPool.emplace_back(0, 1 % length, length - 1);
        for (int i = 1; i < length; ++i)
        {
            m_nodesPool.emplace_back(i, (i + 1) % length, i - 1);
        }
    }

    HeaderList(HeaderList&&) = default;
    HeaderList(const HeaderList&) = delete;
    HeaderList& operator=(const HeaderList&) = delete;

    inline uint16_t length() const { return m_length; }

    inline Header<T>& head() 
    {
        assert(m_length > 0);
        return m_nodesPool[m_headId]; 
    };

    inline Header<T>& get(uint16_t id)
    {
        return m_nodesPool[id];
    }

    Header<T>& eject(uint16_t id)
    {
        m_nodesPool[m_nodesPool[id].m_prevId].m_nextId = m_nodesPool[id].m_nextId;
        m_nodesPool[m_nodesPool[id].m_nextId].m_prevId = m_nodesPool[id].m_prevId;

        --m_length;

        if (m_headId == id)
        {
            m_headId = m_length > 0 ? m_nodesPool[m_headId].m_nextId : INVALID_NODE_ID;
        }

        return m_nodesPool[id];
    }

    void restore(Header<T>& header)
    {
        m_nodesPool[header.m_prevId].m_nextId = header.m_id;
        m_nodesPool[header.m_nextId].m_prevId = header.m_id;

        ++m_length;

        // INVALID_NODE_ID is always bigger than any valid id
        if (header.m_id < m_headId)
        {
            m_headId = header.m_id;
        }
    }
};


using RowHeader = Header<RowType>;
using ColumnHeader = Header<ColumnType>;


struct TableNode
{
    uint16_t m_id;

    uint16_t m_rowId;
    uint16_t m_columnId;

    uint16_t m_leftId  = INVALID_NODE_ID;
    uint16_t m_rightId = INVALID_NODE_ID;
    uint16_t m_upId    = INVALID_NODE_ID;
    uint16_t m_downId  = INVALID_NODE_ID;

    TableNode(uint16_t id, uint16_t rowId, uint16_t columnId)
        : m_id(id)
        , m_rowId(rowId)
        , m_columnId(columnId)
    {
    }

    TableNode(TableNode&&) = default;
    TableNode(const TableNode&) = delete;
    TableNode& operator=(const TableNode&) = delete;
};


class SparseTable
{
public:
    vector<TableNode> m_nodesPool;

    HeaderList<RowType> m_rows;
    HeaderList<ColumnType> m_columns;

    SparseTable(uint16_t rowsCount, uint16_t columnsCount, uint16_t nodesCount)
        : m_rows(rowsCount)
        , m_columns(columnsCount)
    {
        m_nodesPool.reserve(nodesCount);
    }

    SparseTable(const SparseTable&) = delete;
    SparseTable& operator=(const SparseTable&) = delete;

    void reserveNodes(uint16_t nodesCount)
    {
        m_nodesPool.reserve(nodesCount);
    }

    void createNode(uint16_t rowId, uint16_t columnId)
    {
        assert(rowId >= 0 && rowId < m_rows.m_length && columnId >= 0 && columnId < m_columns.m_length);

        const uint16_t nodeId = static_cast<uint16_t>(m_nodesPool.size());
        m_nodesPool.emplace_back(nodeId, rowId, columnId);
        TableNode& node = m_nodesPool.back();

        auto& row = m_rows.get(rowId);
        auto& column = m_columns.get(columnId);

        // Insert into row
        if (row.isEmpty())
        {
            row.m_headNodeId = nodeId;
            node.m_leftId = nodeId;
            node.m_rightId = nodeId;
        }
        else if (m_nodesPool[row.m_headNodeId].m_columnId > columnId)
        {
            // Need to move head to right
            hInsertAfter(nodeId, m_nodesPool[row.m_headNodeId].m_leftId);
            row.m_headNodeId = nodeId;
        }
        else
        {
            uint16_t targetId = row.m_headNodeId;
            while (m_nodesPool[targetId].m_rightId != row.m_headNodeId && m_nodesPool[m_nodesPool[targetId].m_rightId].m_columnId < columnId)
                targetId = m_nodesPool[targetId].m_rightId;

            hInsertAfter(nodeId, targetId);
        }

        // Insert to column
        if (column.isEmpty())
        {
            column.m_headNodeId = nodeId;
            node.m_upId = nodeId;
            node.m_downId = nodeId;
        }
        else if (m_nodesPool[column.m_headNodeId].m_rowId > rowId)
        {
            // Need to move head down
            vInsertAfter(nodeId, m_nodesPool[column.m_headNodeId].m_upId);
            column.m_headNodeId = nodeId;
        }
        else
        {
            uint16_t targetId = column.m_headNodeId;
            while (m_nodesPool[targetId].m_downId != column.m_headNodeId && m_nodesPool[m_nodesPool[targetId].m_downId].m_rowId < rowId)
                targetId = m_nodesPool[targetId].m_downId;

            vInsertAfter(nodeId, targetId);
        }

        ++row.m_nodesCount;
        ++column.m_nodesCount;
    }

    // Insert X horizontally after node Y
    void hInsertAfter(uint16_t xId, uint16_t yId)
    {
        auto& x = m_nodesPool[xId];
        auto& y = m_nodesPool[yId];
     
        x.m_leftId = yId;
        x.m_rightId = y.m_rightId;
        m_nodesPool[x.m_leftId].m_rightId = xId;
        m_nodesPool[x.m_rightId].m_leftId = xId;
    }

    // Insert X vertically after node Y
    void vInsertAfter(uint16_t xId, uint16_t yId)
    {
        auto& x = m_nodesPool[xId];
        auto& y = m_nodesPool[yId];

        x.m_upId = yId;
        x.m_downId = y.m_downId;
        m_nodesPool[x.m_upId].m_downId = xId;
        m_nodesPool[x.m_downId].m_upId = xId;
    }

    void removeFromColumn(uint16_t nodeId)
    {
        auto& node = m_nodesPool[nodeId];

        m_nodesPool[node.m_upId].m_downId = node.m_downId;
        m_nodesPool[node.m_downId].m_upId = node.m_upId;

        // Update head if needed
        auto& column = m_columns.get(node.m_columnId);
        if (column.m_headNodeId == nodeId)
        {
            if (column.m_nodesCount > 1)
                column.m_headNodeId = node.m_downId;
            else
                column.m_headNodeId = INVALID_NODE_ID;
        }

        --column.m_nodesCount;
    }

    void restoreInColumn(uint16_t nodeId)
    {
        auto& node = m_nodesPool[nodeId];

        m_nodesPool[node.m_upId].m_downId = nodeId;
        m_nodesPool[node.m_downId].m_upId = nodeId;

        // Update head if needed
        auto& column = m_columns.get(node.m_columnId);
        if (column.isEmpty() || m_nodesPool[column.m_headNodeId].m_rowId > node.m_rowId)
            column.m_headNodeId = nodeId;

        ++column.m_nodesCount;
    }

    void removeFromRow(uint16_t nodeId)
    {
        auto& node = m_nodesPool[nodeId];

        m_nodesPool[node.m_rightId].m_leftId = node.m_leftId;
        m_nodesPool[node.m_leftId].m_rightId = node.m_rightId;

        // Update head if needed
        auto& row = m_rows.get(node.m_rowId);
        if (row.m_headNodeId == nodeId)
        {
            if (row.m_nodesCount > 1)
                row.m_headNodeId = node.m_rightId;
            else
                row.m_headNodeId = INVALID_NODE_ID;
        }

        --row.m_nodesCount;
    }

    void restoreInRow(uint16_t nodeId)
    {
        auto& node = m_nodesPool[nodeId];

        m_nodesPool[node.m_rightId].m_leftId = nodeId;
        m_nodesPool[node.m_leftId].m_rightId = nodeId;

        // Update head if needed
        auto& row = m_rows.get(node.m_rowId);
        if (row.isEmpty() || m_nodesPool[row.m_headNodeId].m_columnId > node.m_columnId)
            row.m_headNodeId = nodeId;

        ++row.m_nodesCount;
    }

    void ejectColumn(int id)
    {
        ColumnHeader& column = m_columns.eject(id);

        if (column.m_nodesCount > 0)
        {
            uint16_t nodeId = column.m_headNodeId;
            do
            {
                removeFromRow(nodeId);
                nodeId = m_nodesPool[nodeId].m_downId;
            } while (nodeId != column.m_headNodeId);
        }
    }

    void restoreColumn(uint16_t columnId)
    {
        auto& column = m_columns.get(columnId);
        m_columns.restore(column);

        if (column.m_nodesCount > 0)
        {
            uint16_t nodeId = column.m_headNodeId;
            do
            {
                restoreInRow(nodeId);
                nodeId = m_nodesPool[nodeId].m_downId;
            } while (nodeId != column.m_headNodeId);
        }
    }

    void ejectRow(int id)
    {
        RowHeader& row = m_rows.eject(id);

        if (row.m_nodesCount > 0)
        {
            uint16_t nodeId = row.m_headNodeId;
            do
            {
                removeFromColumn(nodeId);
                nodeId = m_nodesPool[nodeId].m_rightId;
            } while (nodeId != row.m_headNodeId);
        }
    }

    void restoreRow(uint16_t rowId)
    {
        auto& row = m_rows.get(rowId);
        m_rows.restore(row);

        if (row.m_nodesCount > 0)
        {
            uint16_t nodeId = row.m_headNodeId;
            do
            {
                restoreInColumn(nodeId);
                nodeId = m_nodesPool[nodeId].m_rightId;
            } while (nodeId != row.m_headNodeId);
        }
    }

    void dumpDebugRepr(uint16_t nodeId, ostream& stream)
    {
        auto& node = m_nodesPool[nodeId];
        auto& left = m_nodesPool[node.m_leftId];
        auto& right = m_nodesPool[node.m_rightId];
        auto& up = m_nodesPool[node.m_upId];
        auto& down = m_nodesPool[node.m_downId];

        stream << "Node (" << node.m_rowId << "; " << node.m_columnId << "): " <<
            "LEFT=("  << left.m_rowId  << "; " << left.m_columnId  << ") " <<
            "RIGHT=(" << right.m_rowId << "; " << right.m_columnId << ") " <<
            "UP=("    << up.m_rowId    << "; " << up.m_columnId    << ") " <<
            "DOWN=("  << down.m_rowId  << "; " << down.m_columnId  << ")" << endl;
    }

    /*
    * Save matrix to file. This is for debug purposes only
    * To be honest, looks quite ugly and uses streams
    */
    void printToFile(string filename)
    {
        ofstream fp(filename, ofstream::out);

        // General information first
        fp << "Matrix size: (" << m_rows.length() << "; " << m_columns.length() << ")" << endl;
        fp << "--------------------" << endl;

        // Rows general information
        {
            uint16_t rowId = m_rows.m_headId;
            do
            {
                auto& rp = m_rows.get(rowId);
                fp << "Row " << rp.m_id << " has " << rp.m_nodesCount << " nodes" << endl;
                rowId = rp.m_nextId;
            } while (rowId != m_rows.m_headId);
        }

        fp << "--------------------" << endl;

        // Columns general information
        {
            uint16_t columnId = m_columns.m_headId;
            do
            {
                auto& cp = m_columns.get(columnId);
                fp << "Column " << cp.m_id << " has " << cp.m_nodesCount << " nodes" << endl;
                columnId = cp.m_nextId;
            } while (columnId != m_columns.m_headId);
        }

        fp << "--------------------" << endl;

        // Detailed nodes dump by rows
        {
            uint16_t rowId = m_rows.m_headId;
            do
            {
                auto& rp = m_rows.get(rowId);
                fp << "Row " << rp.m_id << " nodes:" << endl;

                if (!rp.isEmpty())
                {
                    uint16_t nodeId = rp.m_headNodeId;
                    do
                    {
                        dumpDebugRepr(nodeId, fp);
                        nodeId = m_nodesPool[nodeId].m_rightId;
                    } while (nodeId != rp.m_headNodeId);
                }
                rowId = rp.m_nextId;
            } while (rowId != m_rows.m_headId);
        }

        fp << "--------------------" << endl;

        // Detailed nodes dump by columns
        uint16_t columnId = m_columns.m_headId;
        do
        {
            auto& cp = m_columns.get(columnId);
            fp << "Column " << cp.m_id << " nodes:" << endl;

            if (!cp.isEmpty())
            {
                uint16_t nodeId = cp.m_headNodeId;
                do
                {
                    dumpDebugRepr(nodeId, fp);
                    nodeId = m_nodesPool[nodeId].m_downId;
                } while (nodeId != cp.m_headNodeId);
            }
            columnId = cp.m_nextId;
        } while (columnId != m_columns.m_headId);

        fp.close();
    }
};

class AlgorithmX
{
private:
    SparseTable m_table;
    bool m_finished = false;
    vector<uint16_t> m_finalSolution;

public:
    AlgorithmX(uint16_t setsCount, uint16_t universeSize, uint16_t nodesCount)
        : m_table(setsCount, universeSize, nodesCount)
    {
    }

    void reserveNodes(uint16_t nodesCount)
    {
        m_table.reserveNodes(nodesCount);
    }

    void createNode(uint16_t setId, uint16_t id)
    {
        m_table.createNode(setId, id);
    }

    const vector<uint16_t>& getSolution() const { return m_finalSolution; }

    bool solve()
    {
        assert(!m_finished);

        vector<uint16_t> solution;
        solveIteration(solution);

        // Prevent double execution
        m_finished = true;

        return m_finalSolution.size() != 0;
    }

private:
    struct BackupFrame
    {
        uint16_t m_columnId = INVALID_NODE_ID;
        stack<uint16_t> m_rowIds;
    };

    ColumnHeader* findPivotColumn()
    {
        ColumnHeader* p = &m_table.m_columns.head();
        ColumnHeader* pivot = &m_table.m_columns.head();

        do
        {
            if (p->m_nodesCount < pivot->m_nodesCount)
                pivot = p;
            p = &m_table.m_columns.get(p->m_nextId);
        } while (p->m_id != m_table.m_columns.m_headId);

        return pivot;
    }

    bool solveIteration(vector<uint16_t>& solution)
    {
        if (m_table.m_columns.length() == 0)
        {
            // We have solution

            m_finalSolution = solution;

            return true;
        }

        ColumnHeader* pivotColumn = findPivotColumn();
        if (pivotColumn->m_nodesCount == 0)
            return false;

        RowHeader* pivotRow = &m_table.m_rows.get(m_table.m_nodesPool[pivotColumn->m_headNodeId].m_rowId);
        do
        {
            // Preparations 
            m_table.ejectRow(pivotRow->m_id);
            stack<BackupFrame> backup;

            TableNode* node = &m_table.m_nodesPool[pivotRow->m_headNodeId];
            do
            {
                BackupFrame frame;
                auto& column = m_table.m_columns.get(node->m_columnId);
                if (column.m_nodesCount > 0)
                {
                    TableNode* p = &m_table.m_nodesPool[column.m_headNodeId];
                    while (column.m_nodesCount != 0)
                    {
                        m_table.ejectRow(p->m_rowId);
                        frame.m_rowIds.push(p->m_rowId);
                        p = &m_table.m_nodesPool[p->m_downId];
                    }
                }

                m_table.ejectColumn(node->m_columnId);
                frame.m_columnId = node->m_columnId;
                backup.push(frame);

                node = &m_table.m_nodesPool[node->m_rightId];
            } while (node->m_id != pivotRow->m_headNodeId);

            solution.push_back(pivotRow->m_id);

            bool done = solveIteration(solution);

            solution.pop_back();

            // Restore ejected
            while (!backup.empty())
            {
                BackupFrame frame = backup.top();

                m_table.restoreColumn(frame.m_columnId);
                while (!frame.m_rowIds.empty())
                {
                    m_table.restoreRow(frame.m_rowIds.top());
                    frame.m_rowIds.pop();
                }

                backup.pop();
            }

            m_table.restoreRow(pivotRow->m_id);

            if (done)
                return true;

            pivotRow = &m_table.m_rows.get(pivotRow->m_nextId);
        } while (pivotRow->m_id != m_table.m_nodesPool[pivotColumn->m_headNodeId].m_rowId);

        return false;
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
        {
            for (int j = 0; j < PROBLEM_SIZE; ++j)
            {
                problem[i][j] = data[i + j * PROBLEM_SIZE];
                if (problem[i][j] != 0)
                    ++filledCellsCount;
            }
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

                assert(t >= 0 && t <= PROBLEM_SIZE);

                problem[i][j] = t;

                if (t != 0)
                    ++filledCellsCount;
            }

        fp.close();
    }

    void solve()
    {
        uint16_t variants = PROBLEM_SIZE * PROBLEM_SIZE * PROBLEM_SIZE;
        uint16_t universePower = 4 * PROBLEM_SIZE * PROBLEM_SIZE + filledCellsCount;
        AlgorithmX algo(variants, universePower, universePower);

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

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start);
        double ms = duration.count();

        cout << "Solution took " << ms << " microseconds" << endl;

        // Decoding result
        const vector<uint16_t> solution = algo.getSolution();
        hasSolution = success && solution.size() == PROBLEM_SIZE * PROBLEM_SIZE;
        if (hasSolution)
        {
            for (int i = 0; i < solution.size(); ++i)
            {
                int t = solution[i];
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
