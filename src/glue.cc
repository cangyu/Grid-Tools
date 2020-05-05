#include "../inc/nmf.h"
#include "../inc/plot3d.h"
#include "../inc/xf.h"

static std::string version_str()
{
    static const size_t major = 2;
    static const size_t minor = 0;
    static const size_t patch = 0;

    return "V" + std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

namespace GridTool::XF
{
    MESH::MESH(const std::string &f_nmf, const std::string &f_p3d, std::ostream &fout) :
        DIM(3),
        m_totalNodeNum(0),
        m_totalCellNum(0),
        m_totalFaceNum(0),
        m_totalZoneNum(0)
    {
        /// Load mapping file.
        auto nmf = new NMF::Mapping3D(f_nmf);
        nmf->compute_topology();
        nmf->numbering();

        /// Load grid file.
        auto p3d = new PLOT3D::GRID(f_p3d);

        /// Check consistency.
        const size_t NBLK = nmf->nBlock();
        if (NBLK != p3d->numOfBlock())
            throw std::invalid_argument("Inconsistent num of blocks between NMF and PLOT3D.");
        for (size_t n = 1; n <= NBLK; ++n)
        {
            const auto &b = nmf->block(n);
            auto g = p3d->block(n - 1);
            if (b.IDIM() != g->nI())
                throw std::invalid_argument("Inconsistent num of nodes in I dimension of Block " + std::to_string(n) + ".");
            if (b.JDIM() != g->nJ())
                throw std::invalid_argument("Inconsistent num of nodes in J dimension of Block " + std::to_string(n) + ".");
            if (b.KDIM() != g->nK())
                throw std::invalid_argument("Inconsistent num of nodes in K dimension of Block " + std::to_string(n) + ".");
        }

        /// Allocate storage.
        m_totalNodeNum = nmf->nNode();
        m_totalCellNum = nmf->nCell();
        size_t innerFaceNum = 0, bdryFaceNum = 0;
        nmf->nFace(m_totalFaceNum, innerFaceNum, bdryFaceNum);
        m_node.resize(numOfNode());
        m_face.resize(numOfFace());
        m_cell.resize(numOfCell());

        /// Copy node info.
        std::vector<bool> visited(m_node.size(), false);
        for (size_t n = 1; n <= NBLK; ++n)
        {
            auto &b = nmf->block(n);
            auto &g = *p3d->block(n - 1);

            const size_t nI = b.IDIM();
            const size_t nJ = b.JDIM();
            const size_t nK = b.KDIM();

            for (size_t k = 1; k <= nK; ++k)
                for (size_t j = 1; j <= nJ; ++j)
                    for (size_t i = 1; i <= nI; ++i)
                    {
                        /// Global 1-based index, already assigned.
                        const auto idx = b.node_index(i, j, k);

                        if (!visited[idx - 1])
                        {
                            node(idx).coordinate = g(i, j, k);
                            visited[idx - 1] = true;
                        }
                    }
        }

        /// Copy cell info.
        for (size_t n = 1; n <= NBLK; ++n)
        {
            auto &b = nmf->block(n);

            const size_t nI = b.IDIM();
            const size_t nJ = b.JDIM();
            const size_t nK = b.KDIM();

            for (size_t k = 1; k < nK; ++k)
                for (size_t j = 1; j < nJ; ++j)
                    for (size_t i = 1; i < nI; ++i)
                    {
                        const auto &nc = b.cell(i, j, k);
                        const auto idx = nc.CellSeq();
                        auto &fc = cell(idx);

                        fc.type = CELL::HEXAHEDRAL;

                        fc.includedFace.resize(NMF::Block3D::NumOfSurf);
                        for (short r = 1; r <= NMF::Block3D::NumOfSurf; ++r)
                            fc.includedFace(r) = nc.FaceSeq(r);

                        fc.includedNode.resize(NMF::Block3D::NumOfVertex);
                        for (short r = 1; r <= NMF::Block3D::NumOfVertex; ++r)
                            fc.includedNode(r) = nc.NodeSeq(r);
                    }
        }

        /// Copy face info.
        visited.resize(m_face.size(), false);
        std::fill(visited.begin(), visited.end(), false);
        for (size_t n = 1; n <= NBLK; ++n)
        {
            auto &b = nmf->block(n);

            const size_t nI = b.IDIM();
            const size_t nJ = b.JDIM();
            const size_t nK = b.KDIM();

            /// Internal I direction
            for (size_t k = 1; k < nK; ++k)
                for (size_t j = 1; j < nJ; ++j)
                    for (size_t i = 2; i < nI; ++i)
                    {
                        const auto &curCell = b.cell(i, j, k);
                        const auto &adjCell = b.cell(i - 1, j, k);

                        const auto faceIndex = curCell.FaceSeq(1);
                        auto &curFace = face(faceIndex);

                        curFace.atBdry = false;

                        curFace.type = FACE::QUADRILATERAL;

                        curFace.includedNode.resize(4);
                        curFace.includedNode(1) = curCell.NodeSeq(1);
                        curFace.includedNode(2) = curCell.NodeSeq(5);
                        curFace.includedNode(3) = curCell.NodeSeq(8);
                        curFace.includedNode(4) = curCell.NodeSeq(4);

                        curFace.leftCell = adjCell.CellSeq();
                        curFace.rightCell = curCell.CellSeq();

                        visited[faceIndex - 1] = true;
                    }

            /// Internal J direction
            for (size_t k = 1; k < nK; ++k)
                for (size_t i = 1; i < nI; ++i)
                    for (size_t j = 2; j < nJ; ++j)
                    {
                        const auto &curCell = b.cell(i, j, k);
                        const auto &adjCell = b.cell(i, j - 1, k);

                        const auto faceIndex = curCell.FaceSeq(3);
                        auto &curFace = face(faceIndex);

                        curFace.atBdry = false;

                        curFace.type = FACE::QUADRILATERAL;

                        curFace.includedNode.resize(4);
                        curFace.includedNode(1) = curCell.NodeSeq(6);
                        curFace.includedNode(2) = curCell.NodeSeq(5);
                        curFace.includedNode(3) = curCell.NodeSeq(1);
                        curFace.includedNode(4) = curCell.NodeSeq(2);

                        curFace.leftCell = adjCell.CellSeq();
                        curFace.rightCell = curCell.CellSeq();

                        visited[faceIndex - 1] = true;
                    }

            /// Internal k direction
            for (size_t i = 1; i < nI; ++i)
                for (size_t j = 1; j < nJ; ++j)
                    for (size_t k = 2; k < nK; ++k)
                    {
                        const auto &curCell = b.cell(i, j, k);
                        const auto &adjCell = b.cell(i, j, k - 1);

                        const auto faceIndex = curCell.FaceSeq(5);
                        auto &curFace = face(faceIndex);

                        curFace.atBdry = false;

                        curFace.type = FACE::QUADRILATERAL;

                        curFace.includedNode.resize(4);
                        curFace.includedNode(1) = curCell.NodeSeq(4);
                        curFace.includedNode(2) = curCell.NodeSeq(3);
                        curFace.includedNode(3) = curCell.NodeSeq(2);
                        curFace.includedNode(4) = curCell.NodeSeq(1);

                        curFace.leftCell = adjCell.CellSeq();
                        curFace.rightCell = curCell.CellSeq();

                        visited[faceIndex - 1] = true;
                    }

            /// I-MIN
            for (size_t k = 1; k < nK; ++k)
                for (size_t j = 1; j < nJ; ++j)
                {
                    const auto &curCell = b.cell(1, j, k);
                    const auto faceIndex = curCell.FaceSeq(1);
                    auto &curFace = face(faceIndex);
                    const auto &curSurf = b.surf(1);

                    if (visited[faceIndex - 1])
                    {
                        if (!curFace.atBdry)
                        {
                            /// Second-Round of Double-Sided Case
                            /// Update undetermined cell index.
                            if (curFace.leftCell == 0)
                                curFace.leftCell = curCell.CellSeq();
                            else if (curFace.rightCell == 0)
                                curFace.rightCell = curCell.CellSeq();
                            else
                                throw std::runtime_error("Double-Sided face should not appear more than twice!");
                        }
                        else
                            throw std::runtime_error("Boundary face shouldn't appear twice!");
                    }
                    else
                    {
                        curFace.type = FACE::QUADRILATERAL;

                        curFace.atBdry = !curSurf.neighbourSurf;

                        curFace.includedNode.resize(4);
                        curFace.includedNode(1) = curCell.NodeSeq(1);
                        curFace.includedNode(2) = curCell.NodeSeq(5);
                        curFace.includedNode(3) = curCell.NodeSeq(8);
                        curFace.includedNode(4) = curCell.NodeSeq(4);

                        /// On I-MIN Surface, if current face is Single-Sided,
                        /// then index of left cell is set to 0 according to
                        /// right-hand convention; If current face is Double-Sided,
                        /// it is also set to 0 at this stage, and will be
                        /// updated further by loops of other blocks.
                        curFace.leftCell = 0;
                        curFace.rightCell = curCell.CellSeq();

                        visited[faceIndex - 1] = true;
                    }
                }

            /// I-MAX
            for (size_t k = 1; k < nK; ++k)
                for (size_t j = 1; j < nJ; ++j)
                {
                    const auto &curCell = b.cell(nI - 1, j, k);
                    const auto faceIndex = curCell.FaceSeq(2);
                    auto &curFace = face(faceIndex);
                    const auto &curSurf = b.surf(2);

                    if (visited[faceIndex - 1])
                    {
                        if (!curFace.atBdry)
                        {
                            /// Second-Round of Double-Sided Case
                            /// Update undetermined cell index.
                            if (curFace.leftCell == 0)
                                curFace.leftCell = curCell.CellSeq();
                            else if (curFace.rightCell == 0)
                                curFace.rightCell = curCell.CellSeq();
                            else
                                throw std::runtime_error("Double-Sided face should not appear more than twice!");
                        }
                        else
                            throw std::runtime_error("Boundary face shouldn't appear twice!");
                    }
                    else
                    {
                        curFace.type = FACE::QUADRILATERAL;

                        curFace.atBdry = !curSurf.neighbourSurf;

                        curFace.includedNode.resize(4);
                        curFace.includedNode(1) = curCell.NodeSeq(2);
                        curFace.includedNode(2) = curCell.NodeSeq(3);
                        curFace.includedNode(3) = curCell.NodeSeq(7);
                        curFace.includedNode(4) = curCell.NodeSeq(6);

                        /// On I-MAX Surface, if current face is Single-Sided,
                        /// then index of left cell is set to 0 according to
                        /// right-hand convention; If current face is Double-Sided,
                        /// it is also set to 0 at this stage, and will be
                        /// updated further by loops of other blocks.
                        curFace.leftCell = 0;
                        curFace.rightCell = curCell.CellSeq();

                        visited[faceIndex - 1] = true;
                    }
                }

            /// J-MIN
            for (size_t k = 1; k < nK; ++k)
                for (size_t i = 1; i < nI; ++i)
                {
                    const auto &curCell = b.cell(i, 1, k);
                    const auto faceIndex = curCell.FaceSeq(3);
                    auto &curFace = face(faceIndex);
                    const auto &curSurf = b.surf(3);

                    if (visited[faceIndex - 1])
                    {
                        if (!curFace.atBdry)
                        {
                            /// Second-Round of Double-Sided Case
                            /// Update undetermined cell index.
                            if (curFace.leftCell == 0)
                                curFace.leftCell = curCell.CellSeq();
                            else if (curFace.rightCell == 0)
                                curFace.rightCell = curCell.CellSeq();
                            else
                                throw std::runtime_error("Double-Sided face should not appear more than twice!");
                        }
                        else
                            throw std::runtime_error("Boundary face shouldn't appear twice!");
                    }
                    else
                    {
                        curFace.type = FACE::QUADRILATERAL;

                        curFace.atBdry = !curSurf.neighbourSurf;

                        curFace.includedNode.resize(4);
                        curFace.includedNode(1) = curCell.NodeSeq(6);
                        curFace.includedNode(2) = curCell.NodeSeq(5);
                        curFace.includedNode(3) = curCell.NodeSeq(1);
                        curFace.includedNode(4) = curCell.NodeSeq(2);

                        /// On J-MIN Surface, if current face is Single-Sided,
                        /// then index of left cell is set to 0 according to
                        /// right-hand convention; If current face is Double-Sided,
                        /// it is also set to 0 at this stage, and will be
                        /// updated further by loops of other blocks.
                        curFace.leftCell = 0;
                        curFace.rightCell = curCell.CellSeq();

                        visited[faceIndex - 1] = true;
                    }
                }

            /// J-MAX
            for (size_t k = 1; k < nK; ++k)
                for (size_t i = 1; i < nI; ++i)
                {
                    const auto &curCell = b.cell(i, nJ - 1, k);
                    const auto faceIndex = curCell.FaceSeq(4);
                    auto &curFace = face(faceIndex);
                    const auto &curSurf = b.surf(4);

                    if (visited[faceIndex - 1])
                    {
                        if (!curFace.atBdry)
                        {
                            /// Second-Round of Double-Sided Case
                            /// Update undetermined cell index.
                            if (curFace.leftCell == 0)
                                curFace.leftCell = curCell.CellSeq();
                            else if (curFace.rightCell == 0)
                                curFace.rightCell = curCell.CellSeq();
                            else
                                throw std::runtime_error("Double-Sided face should not appear more than twice!");
                        }
                        else
                            throw std::runtime_error("Boundary face shouldn't appear twice!");
                    }
                    else
                    {
                        curFace.type = FACE::QUADRILATERAL;

                        curFace.atBdry = !curSurf.neighbourSurf;

                        curFace.includedNode.resize(4);
                        curFace.includedNode(1) = curCell.NodeSeq(3);
                        curFace.includedNode(2) = curCell.NodeSeq(4);
                        curFace.includedNode(3) = curCell.NodeSeq(8);
                        curFace.includedNode(4) = curCell.NodeSeq(7);

                        /// On J-MAX Surface, if current face is Single-Sided,
                        /// then index of left cell is set to 0 according to
                        /// right-hand convention; If current face is Double-Sided,
                        /// it is also set to 0 at this stage, and will be
                        /// updated further by loops of other blocks.
                        curFace.leftCell = 0;
                        curFace.rightCell = curCell.CellSeq();

                        visited[faceIndex - 1] = true;
                    }
                }

            /// K-MIN
            for (size_t i = 1; i < nI; ++i)
                for (size_t j = 1; j < nJ; ++j)
                {
                    const auto &curCell = b.cell(i, j, 1);
                    const auto faceIndex = curCell.FaceSeq(5);
                    auto &curFace = face(faceIndex);
                    const auto &curSurf = b.surf(5);

                    if (visited[faceIndex - 1])
                    {
                        if (!curFace.atBdry)
                        {
                            /// Second-Round of Double-Sided Case
                            /// Update undetermined cell index.
                            if (curFace.leftCell == 0)
                                curFace.leftCell = curCell.CellSeq();
                            else if (curFace.rightCell == 0)
                                curFace.rightCell = curCell.CellSeq();
                            else
                                throw std::runtime_error("Double-Sided face should not appear more than twice!");
                        }
                        else
                            throw std::runtime_error("Boundary face shouldn't appear twice!");
                    }
                    else
                    {
                        curFace.type = FACE::QUADRILATERAL;

                        curFace.atBdry = !curSurf.neighbourSurf;

                        curFace.includedNode.resize(4);
                        curFace.includedNode(1) = curCell.NodeSeq(4);
                        curFace.includedNode(2) = curCell.NodeSeq(3);
                        curFace.includedNode(3) = curCell.NodeSeq(2);
                        curFace.includedNode(4) = curCell.NodeSeq(1);

                        /// On K-MIN Surface, if current face is Single-Sided,
                        /// then index of left cell is set to 0 according to
                        /// right-hand convention; If current face is Double-Sided,
                        /// it is also set to 0 at this stage, and will be
                        /// updated further by loops of other blocks.
                        curFace.leftCell = 0;
                        curFace.rightCell = curCell.CellSeq();

                        visited[faceIndex - 1] = true;
                    }
                }

            /// K-MAX
            for (size_t i = 1; i < nI; ++i)
                for (size_t j = 1; j < nJ; ++j)
                {
                    const auto &curCell = b.cell(i, j, nK - 1);
                    const auto faceIndex = curCell.FaceSeq(6);
                    auto &curFace = face(faceIndex);
                    const auto &curSurf = b.surf(6);

                    if (visited[faceIndex - 1])
                    {
                        if (!curFace.atBdry)
                        {
                            /// Second-Round of Double-Sided Case
                            /// Update undetermined cell index.
                            if (curFace.leftCell == 0)
                                curFace.leftCell = curCell.CellSeq();
                            else if (curFace.rightCell == 0)
                                curFace.rightCell = curCell.CellSeq();
                            else
                                throw std::runtime_error("Double-Sided face should not appear more than twice!");
                        }
                        else
                            throw std::runtime_error("Boundary face shouldn't appear twice!");
                    }
                    else
                    {
                        curFace.type = FACE::QUADRILATERAL;

                        curFace.atBdry = !curSurf.neighbourSurf;

                        curFace.includedNode.resize(4);
                        curFace.includedNode(1) = curCell.NodeSeq(8);
                        curFace.includedNode(2) = curCell.NodeSeq(5);
                        curFace.includedNode(3) = curCell.NodeSeq(6);
                        curFace.includedNode(4) = curCell.NodeSeq(7);

                        /// On K-MAX Surface, if current face is Single-Sided,
                        /// then index of left cell is set to 0 according to
                        /// right-hand convention; If current face is Double-Sided,
                        /// it is also set to 0 at this stage, and will be
                        /// updated further by loops of other blocks.
                        curFace.leftCell = 0;
                        curFace.rightCell = curCell.CellSeq();

                        visited[faceIndex - 1] = true;
                    }
                }
        }

        /// Assign ZONE info.
        /// TODO

        /// Convert to primary form.
        derived2raw();

        /// Finalize.
        delete nmf;
        delete p3d;
    }

    void MESH::derived2raw()
    {
        clear_entry();

        add_entry(new HEADER("Block-Glue " + version_str()));
        add_entry(new DIMENSION(3));

        auto pnt = new NODE(1, 1, numOfNode(), NODE::ANY, 3);

        /// TODO
    }
}
