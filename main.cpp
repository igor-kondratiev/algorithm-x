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


const uint16_t INVALID_NODE_ID = numeric_limits<uint16_t>::max();

enum HeaderType
{
    RowType,
    ColumnType
};

#pragma pack(push,1)
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

    // Non-copyable, but movable to allow 
    // storing nodes in a pre-allocated vector
    Header(Header&&) = default;
    Header(const Header&) = delete;
    Header& operator=(const Header&) = delete;

    inline bool isEmpty() const { return m_nodesCount == 0; }
};
#pragma pack(pop)


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

    inline Header<T>& eject(uint16_t id)
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

    inline void restore(Header<T>& header)
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


#pragma pack(push,1)
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
#pragma pack(pop)


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

    inline void removeFromColumn(uint16_t nodeId)
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

    inline void restoreInColumn(uint16_t nodeId)
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

    inline void removeFromRow(uint16_t nodeId)
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

    inline void restoreInRow(uint16_t nodeId)
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

    inline void ejectColumn(int id)
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

    inline void restoreColumn(uint16_t columnId)
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

    inline void ejectRow(int id)
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

    inline void restoreRow(uint16_t rowId)
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

    inline void createNode(uint16_t setId, uint16_t id)
    {
        m_table.createNode(setId, id);
    }

    inline const vector<uint16_t>& getSolution() const { return m_finalSolution; }

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
        vector<uint16_t> m_rowIds;
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

        uint16_t startingPivotRowId = m_table.m_nodesPool[pivotColumn->m_headNodeId].m_rowId;
        RowHeader* pivotRow = &m_table.m_rows.get(startingPivotRowId);
        do
        {
            // Preparations 
            m_table.ejectRow(pivotRow->m_id);
            vector<BackupFrame> backup;
            backup.reserve(pivotRow->m_nodesCount);

            TableNode* node = &m_table.m_nodesPool[pivotRow->m_headNodeId];
            do
            {
                backup.emplace_back();
                BackupFrame& frame = backup.back();
                auto& column = m_table.m_columns.get(node->m_columnId);
                if (column.m_nodesCount > 0)
                {
                    frame.m_rowIds.reserve(column.m_nodesCount);

                    TableNode* p = &m_table.m_nodesPool[column.m_headNodeId];
                    while (column.m_nodesCount != 0)
                    {
                        m_table.ejectRow(p->m_rowId);
                        frame.m_rowIds.push_back(p->m_rowId);
                        p = &m_table.m_nodesPool[p->m_downId];
                    }
                }

                m_table.ejectColumn(node->m_columnId);
                frame.m_columnId = node->m_columnId;

                node = &m_table.m_nodesPool[node->m_rightId];
            } while (node->m_id != pivotRow->m_headNodeId);

            solution.push_back(pivotRow->m_id);

            bool done = solveIteration(solution);
            if (done)
                return true;

            solution.pop_back();

            // Restore ejected
            for (int i = backup.size() - 1; i >= 0; --i)
            {
                BackupFrame frame = backup[i];

                m_table.restoreColumn(frame.m_columnId);
                for (int j = frame.m_rowIds.size() - 1; j >= 0; --j)
                {
                    m_table.restoreRow(frame.m_rowIds[j]);
                }
            }

            m_table.restoreRow(pivotRow->m_id);

            pivotRow = &m_table.m_rows.get(pivotRow->m_nextId);
        } while (pivotRow->m_id != startingPivotRowId);

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

    SudokuProblem(const string& data)
        : problem(PROBLEM_SIZE, vector<int>(PROBLEM_SIZE, 0)), solvedProblem(PROBLEM_SIZE, vector<int>(PROBLEM_SIZE, 0))
    {
        for (int i = 0; i < PROBLEM_SIZE; ++i)
        {
            for (int j = 0; j < PROBLEM_SIZE; ++j)
            {
                problem[i][j] = data[i * PROBLEM_SIZE + j] == '.' ? 0 : data[i * PROBLEM_SIZE + j] - '0';
                if (problem[i][j] != 0)
                    ++filledCellsCount;
            }
        }
    }

    void solve()
    {
        uint16_t variants = PROBLEM_SIZE * PROBLEM_SIZE * PROBLEM_SIZE;
        uint16_t universePower = 4 * PROBLEM_SIZE * PROBLEM_SIZE + filledCellsCount;
        uint16_t nodesCount = 4 * PROBLEM_SIZE * PROBLEM_SIZE * PROBLEM_SIZE + filledCellsCount;
        AlgorithmX algo(variants, universePower, nodesCount);

        // Preparations
        // Row-Column constraints first
        for (int i = 0; i < PROBLEM_SIZE; ++i)
            for (int j = 0; j < PROBLEM_SIZE; ++j)
                for (int v = 0; v < PROBLEM_SIZE; ++v)
                    algo.createNode(packRowID(i, j, v), ROW_COL_OFFSET + packColID(i, j));

        // Row-Number constraints
        for (int i = 0; i < PROBLEM_SIZE; ++i)
            for (int v = 0; v < PROBLEM_SIZE; ++v)
                for (int j = 0; j < PROBLEM_SIZE; ++j)
                    algo.createNode(packRowID(i, j, v), ROW_NUM_OFFSET + packColID(i, v));

        // Column-Number constraints
        for (int j = 0; j < PROBLEM_SIZE; ++j)
            for (int v = 0; v < PROBLEM_SIZE; ++v)
                for (int i = 0; i < PROBLEM_SIZE; ++i)
                    algo.createNode(packRowID(i, j, v), COL_NUM_OFFSET + packColID(j, v));

        // Box-Number constraints
        for (int v = 0; v < PROBLEM_SIZE; ++v)
            for (int i = 0; i < PROBLEM_SIZE; ++i)
                for (int j = 0; j < PROBLEM_SIZE; ++j)
                    algo.createNode(packRowID(i, j, v), BOX_NUM_OFFSET + packColID(getBoxID(i, j), v));

        // Filled numbers constraints
        int counter = 0;
        for (int i = 0; i < PROBLEM_SIZE; ++i)
            for (int j = 0; j < PROBLEM_SIZE; ++j)
            {
                if (problem[i][j] != 0)
                    algo.createNode(packRowID(i, j, problem[i][j] - 1), FILLED_NUM_OFFSET + counter++);
            }

        

        bool success = algo.solve();

        // cout << "Solution was successfull: " << success << endl;

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

int main()
{
    // Benchmarking based on easy kaggle set
    // ifstream benchmark_input("test_sudoku_full.txt");
    ifstream benchmark_input("test_sudoku.txt");

    auto start = std::chrono::steady_clock::now();

    int problemsCount = 0;
    while (!benchmark_input.eof())
    {
        string input;
        benchmark_input >> input;
        if (input.size() != 81)
            continue;

        SudokuProblem p(input);
        p.solve();

        ++problemsCount;
    }

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    double ms = duration.count();

    cout << "Solution took " << ms << " milliseconds" << endl;
    cout << "Puzzles/sec " << problemsCount / (ms / 1000) << endl;

    cin.get();

    return 0;
}
