/** -*-c++-*-
 *
 *  Copyright 2014 The Australian National University
 *
 *  testTraversal.C
 *
 *  Tests vector field traversals and derived algorithms.
 *
 *  Olaf Delgado-Friedrichs jan 14
 *
 */

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "generative.hpp"
#include "common.hpp"
#include "booster.hpp"

#include "restricted.hpp"


using namespace anu_am::generative;
using namespace anu_am::generative::booster;
using namespace anu_am::diamorse;


// === Helpers.

typedef std::map<Cell, std::set<Cell> > Labels;


template<class Cell, class VectorField, class Incidences>
Labels markCells(
    std::vector<Cell> const& sources,
    VectorField const& V,
    Incidences  const& I)
{
    Labels result;

    for (size_t i = 0; i < sources.size(); ++i)
    {
        Cell const s = sources.at(i);
        result[s].insert(s);

        std::vector<std::pair<Cell, Cell> > t = flowTraversal(s, V, I);

        for (size_t j = 0; j < t.size(); ++j)
        {
            Cell const b = t.at(j).second;
            Cell const c = V(b);
            if (b != c)
                result[c].insert(s);
        }
    }

    return result;
}


Labels markCells(
    VolumeData const& candidate,
    bool const upstream)
{
    CubicalComplex const& complex = candidate.complex;
    Field const& field = candidate.field;

    int const xd = complex.xdim();
    int const yd = complex.ydim();
    int const zd = complex.zdim();

    std::vector<Cell> sources;
    for (Cell v = 0; v < complex.cellIdLimit(); ++v)
        if (complex.isCell(v) and field.isCritical(v))
            sources.push_back(v);

    return markCells(sources,
                     upstream ? field.coV() : field.V(),
                     Facets(xd, yd, zd, upstream));
}


// === Tests start here.

Result criticalCellsAreSelfLabelled(VolumeData const& candidate)
{
    CubicalComplex const& complex = candidate.complex;
    Field const& field = candidate.field;

    for (int up = 0; up <= 1; ++up)
    {
        Labels const labels = markCells(candidate, up);
        for (Cell v = 0; v < complex.cellIdLimit(); ++v)
        {
            if (complex.isCell(v) and field.isCritical(v))
            {
                if (labels.count(v) == 0 or labels.at(v).count(v) < 1)
                {
                    std::stringstream msg;
                    msg << "Critical cell " << complex.cellPosition(v)
                        << " is not in its own " << (up ? "unstable" : "stable")
                        << " set";
                    return failure(msg.str());
                }
                else if (labels.at(v).size() > 1)
                {
                    std::stringstream msg;
                    msg << "Critical cell " << complex.cellPosition(v)
                        << " is in other " << (up ? "unstable" : "stable")
                        << " sets";
                    return failure(msg.str());
                }
            }
        }
    }

    return success();
}

SIMPLE_TEST_CASE(
    criticalCellsAreInExactlyTheirOwnStableOrUnstableSets,
    checkWithVolumeData(criticalCellsAreSelfLabelled))


Result labelsPropagate(
    Labels const& labels,
    Cell const v,
    Cell const w,
    bool const upstream,
    CubicalComplex const& complex)
{
    std::set<Cell> const& lv = labels.at(v);
    std::set<Cell> const& lw = labels.at(w);

    std::set<Cell>::const_iterator iter;
    for (iter = lv.begin(); iter != lv.end(); ++iter)
    {
        if (lw.count(*iter) == 0)
        {
            std::stringstream msg;
            msg << (upstream ? "unstable" : "stable") << " sets for cell "
                << complex.cellPosition(v)
                << " are not contained in those for "
                << complex.cellPosition(w)
                << ": {" << lv << "} vs {" << lw << "}";
            return failure(msg.str());
        }
    }
    return success();
}

Result traversalsAreConsistent(VolumeData const& candidate)
{
    CubicalComplex const& complex = candidate.complex;
    Field const& field = candidate.field;

    for (int up = 0; up <= 1; ++up)
    {
        Labels const labels = markCells(candidate, up);
        Field::Vectors V = up ? field.coV() : field.V();
        Facets I(complex.xdim(), complex.ydim(), complex.zdim(), up);

        for (Cell v = 0; v < complex.cellIdLimit(); ++v)
        {
            if (not complex.isCell(v))
                continue;

            if (labels.count(v) > 0)
            {
                int const n = I.count(v);
                for (int i = 0; i < n; ++i) {
                    Cell const b = I(v, i);

                    if (not V.defined(b) or V(b) == b)
                        continue;

                    Result r = labelsPropagate(labels, v, V(b), up, complex);
                    if (!r)
                        return r;
                }
            }
        }
    }
    return success();
}

SIMPLE_TEST_CASE(
    allStableAndUnstableSetsAreConsistent,
    checkWithVolumeData(traversalsAreConsistent))


Result checkLabelCounts(
    VolumeData const& candidate,
    int const dim,
    bool const upstream,
    int const min = 1,
    int const max = 1)
{
    CubicalComplex const& complex = candidate.complex;
    Labels const labels = markCells(candidate, upstream);

    for (Cell v = 0; v < complex.cellIdLimit(); ++v)
    {
        if (complex.isCell(v) and complex.cellDimension(v) == dim)
        {
            int const count = labels.count(v) == 0 ? 0 : labels.at(v).size();

            if (count < min)
            {
                std::stringstream msg;
                msg << "Cell " << complex.cellPosition(v);
                if (min == 1)
                    msg << " has no label";
                else
                    msg << " has too few labels (" << count << ")";
                return failure(msg.str());
            }
            else if (count > max)
            {
                std::stringstream msg;
                msg << "Cell " << complex.cellPosition(v)
                    << " has too many labels (" << count << ")";
                return failure(msg.str());
            }
        }
    }

    return success();
}


Result checkVertexLabelCounts(VolumeData const& candidate)
{
    return checkLabelCounts(candidate, 0, true, 1, 1);
}


SIMPLE_TEST_CASE(
    theBasinsPartitionTheVertexSet,
    checkWithVolumeData(checkVertexLabelCounts))


Result checkMaxCellLabelsCounts(VolumeData const& candidate)
{
    int const d = candidate.complex.dimension();
    return checkLabelCounts(candidate, d, false, 0, 1);
}


SIMPLE_TEST_CASE(
    cellsOfMaximumDimensionAreInAtMostOneStableSet,
    checkWithVolumeData(checkMaxCellLabelsCounts))


Result checkMorseBoundaryMethods(VolumeData const& candidate)
{
    CubicalComplex const& complex = candidate.complex;
    Field const& field = candidate.field;
    Field::Vectors const& V = field.V();
    Facets I(complex.xdim(), complex.ydim(), complex.zdim(), false);
    std::map<Cell, Boundary> chains = chainComplex(complex, field);
    
    for (Cell c = 0; c < complex.cellIdLimit(); ++c)
    {
        if (complex.isCell(c) and field.isCritical(c))
        {
            Boundary const B1 = morseBoundary(c, V, I);
            Boundary const B2 = morseBoundaryFast(c, V, I);
            Boundary const B3 = chains.at(c);

            if (B1 != B2 or B1 != B3) {
                std::stringstream msg;
                msg << "Mismatching Morse boundaries for cell "
                    << complex.cellPosition(c) << ": "
                    << B1 << " vs " << B2 << " vs " << B3;
                return failure(msg.str());
            }
        }
    }

    return success();
}

SIMPLE_TEST_CASE(
    theTwoMorseBoundaryAlgorithmsYieldIdenticalResults,
    checkWithVolumeData(checkMorseBoundaryMethods))


Result checkMorseBoundariesUpstreamVersusDownstream(VolumeData const& candidate)
{
    CubicalComplex const& complex = candidate.complex;
    Field const& field = candidate.field;

    Field::Vectors const& V = field.V();
    Field::Vectors const& coV = field.coV();

    Facets I(complex.xdim(), complex.ydim(), complex.zdim(), false);
    Facets coI(complex.xdim(), complex.ydim(), complex.zdim(), true);

    std::map<Cell, Boundary> M1;
    std::map<Cell, Boundary> M2;

    for (Cell c = 0; c < complex.cellIdLimit(); ++c)
    {
        if (complex.isCell(c) and field.isCritical(c))
        {
            Boundary B = morseBoundary(c, V, I);
            Boundary coB = morseBoundary(c, coV, coI);

            M1[c] = B;

            for (size_t i = 0; i < coB.size(); ++i)
            {
                Cell const d = coB.at(i).first;
                int const n = coB.at(i).second;
                M2[d].push_back(std::pair<Cell, int>(c, n));
            }
        }
    }

    for (Cell c = 0; c < complex.cellIdLimit(); ++c)
    {
        if (complex.isCell(c) and field.isCritical(c))
        {
            if (M1[c] != M2[c]) {
                std::stringstream msg;
                msg << "Mismatching Morse boundaries for cell "
                    << complex.cellPosition(c)
                    << ": " << M1[c] << " vs " << M2[c];
                return failure(msg.str());
            }
        }
    }
    return success();
}

SIMPLE_TEST_CASE(
    morseComplexesComputedUpstreamAndDownstreamCoincide,
    checkWithVolumeData(checkMorseBoundariesUpstreamVersusDownstream))


Result checkCubicalVersusChainHomology(VolumeData const& candidate)
{
    return checkPersistentHomology(convertedCubicalComplex(candidate),
                                   convertedChainComplex(candidate));
}

SIMPLE_TEST_CASE(
    cubicalAndChainComplexHaveTheSamePersistentHomoloy,
    checkWithVolumeData(checkCubicalVersusChainHomology))


Result checkConnections(VolumeData const& candidate)
{
    CubicalComplex const& complex = candidate.complex;
    Field const& field = candidate.field;

    Field::Vectors const V = field.V();
    Field::Vectors const coV = field.coV();
    Facets const I(complex.xdim(), complex.ydim(), complex.zdim(), false);
    Facets const coI(complex.xdim(), complex.ydim(), complex.zdim(), true);

    std::vector<Cell> const sources = criticalCells(candidate);

    boost::shared_ptr<std::vector<bool> > marked =
        connectingPaths(complex, field, V, coV, I, coI);

    for (size_t v = 0; v < complex.cellIdLimit(); ++v)
    {
        if (not marked->at(v))
        {
            if (complex.isCell(v) and field.isCritical(v))
            {
                std::stringstream msg;
                msg << "unmarked critical cell at " << complex.cellPosition(v);
                return failure(msg.str());
            }
        }
        else
        {
            bool good = false;

            if (field.isCritical(v))
            {
                continue;
            }
            else if (V.defined(v))
            {
                Cell const u = V(v);
                for (int i = 0; i < coI.count(v); ++i)
                {
                    Cell const w = coI(v, i);
                    good = good || (w != u and marked->at(w));
                }
            }
            else
            {
                Cell const u = coV(v);
                for (int i = 0; i < I.count(v); ++i)
                {
                    Cell const w = I(v, i);
                    good = good || (w != u and marked->at(w));
                }
            }

            if (not good)
            {
                std::stringstream msg;
                msg << "dead end at " << complex.cellPosition(v);
                return failure(msg.str());
            }
        }
    }

    return success();
}

SIMPLE_TEST_CASE(
    connectionsAreConsistent,
    checkWithVolumeData(checkConnections))
