/*
 * Distributed under the OSI-approved Apache License, Version 2.0.  See
 * accompanying file Copyright.txt for details.
 *
 * SscWriter.cpp
 *
 *  Created on: Nov 1, 2018
 *      Author: Jason Wang
 */

#include "SscWriter.tcc"
#include "adios2/helper/adiosCommMPI.h"

namespace adios2
{
namespace core
{
namespace engine
{

SscWriter::SscWriter(IO &io, const std::string &name, const Mode mode,
                     helper::Comm comm)
: Engine("SscWriter", io, name, mode, std::move(comm))
{
    PERFSTUBS_SCOPED_TIMER_FUNC();

    helper::GetParameter(m_IO.m_Parameters, "Verbose", m_Verbosity);
    helper::GetParameter(m_IO.m_Parameters, "Threading", m_Threading);
    helper::GetParameter(m_IO.m_Parameters, "OpenTimeoutSecs",
                         m_OpenTimeoutSecs);

    helper::Log("Engine", "SSCWriter", "Open", m_Name, 0, m_Comm.Rank(), 5,
                m_Verbosity, helper::LogMode::INFO);

    int providedMpiMode;
    MPI_Query_thread(&providedMpiMode);
    if (m_Threading && providedMpiMode != MPI_THREAD_MULTIPLE)
    {
        m_Threading = false;
        helper::Log("Engine", "SSCWriter", "Open",
                    "SSC Threading disabled as MPI is not initialized with "
                    "multi-threads",
                    0, m_Comm.Rank(), 1, m_Verbosity, helper::LogMode::WARNING);
    }

    SyncMpiPattern();
}

StepStatus SscWriter::BeginStep(StepMode mode, const float timeoutSeconds)
{
    PERFSTUBS_SCOPED_TIMER_FUNC();

    if (m_Threading && m_EndStepThread.joinable())
    {
        m_EndStepThread.join();
    }

    ++m_CurrentStep;

    helper::Log("Engine", "SSCWriter", "BeginStep",
                std::to_string(CurrentStep()), 0, m_Comm.Rank(), 5, m_Verbosity,
                helper::LogMode::INFO);

    if (m_CurrentStep == 0 || m_WriterDefinitionsLocked == false ||
        m_ReaderSelectionsLocked == false)
    {
        m_Buffer.resize(1);
        m_Buffer[0] = 0;
        m_GlobalWritePattern.clear();
        m_GlobalWritePattern.resize(m_StreamSize);
        m_GlobalReadPattern.clear();
        m_GlobalReadPattern.resize(m_StreamSize);
    }

    if (m_CurrentStep > 1)
    {
        if (m_WriterDefinitionsLocked && m_ReaderSelectionsLocked)
        {
            MPI_Waitall(static_cast<int>(m_MpiRequests.size()),
                        m_MpiRequests.data(), MPI_STATUSES_IGNORE);
            m_MpiRequests.clear();
        }
        else
        {
            MPI_Win_free(&m_MpiWin);
        }
    }

    return StepStatus::OK;
}

size_t SscWriter::CurrentStep() const { return m_CurrentStep; }

void SscWriter::PerformPuts()
{
    PERFSTUBS_SCOPED_TIMER_FUNC();
    helper::Log("Engine", "SSCWriter", "PerformPuts", "", 0, m_Comm.Rank(), 5,
                m_Verbosity, helper::LogMode::INFO);
}

void SscWriter::EndStepFirst()
{
    PERFSTUBS_SCOPED_TIMER_FUNC();

    SyncWritePattern();
    MPI_Win_create(m_Buffer.data(), m_Buffer.size(), 1, MPI_INFO_NULL,
                   m_StreamComm, &m_MpiWin);
    MPI_Win_free(&m_MpiWin);
    SyncReadPattern();
}

void SscWriter::EndStepConsequentFixed()
{
    PERFSTUBS_SCOPED_TIMER_FUNC();
    for (const auto &i : m_AllSendingReaderRanks)
    {
        m_MpiRequests.emplace_back();
        MPI_Isend(m_Buffer.data(), static_cast<int>(m_Buffer.size()), MPI_CHAR,
                  i.first, 0, m_StreamComm, &m_MpiRequests.back());
    }
}

void SscWriter::EndStepConsequentFlexible()
{
    PERFSTUBS_SCOPED_TIMER_FUNC();
    SyncWritePattern();
    MPI_Win_create(m_Buffer.data(), m_Buffer.size(), 1, MPI_INFO_NULL,
                   m_StreamComm, &m_MpiWin);
}

void SscWriter::EndStep()
{
    PERFSTUBS_SCOPED_TIMER_FUNC();

    helper::Log("Engine", "SSCWriter", "EndStep", std::to_string(CurrentStep()),
                0, m_Comm.Rank(), 5, m_Verbosity, helper::LogMode::INFO);

    if (m_CurrentStep == 0)
    {
        if (m_Threading)
        {
            m_EndStepThread = std::thread(&SscWriter::EndStepFirst, this);
        }
        else
        {
            EndStepFirst();
        }
    }
    else
    {
        if (m_WriterDefinitionsLocked && m_ReaderSelectionsLocked)
        {
            EndStepConsequentFixed();
        }
        else
        {
            if (m_Threading)
            {
                m_EndStepThread =
                    std::thread(&SscWriter::EndStepConsequentFlexible, this);
            }
            else
            {
                EndStepConsequentFlexible();
            }
        }
    }
}

void SscWriter::Flush(const int transportIndex)
{
    PERFSTUBS_SCOPED_TIMER_FUNC();
}

void SscWriter::SyncMpiPattern()
{
    PERFSTUBS_SCOPED_TIMER_FUNC();

    helper::Log("Engine", "SSCWriter", "SyncMpiPattern", "", 0, m_Comm.Rank(),
                5, m_Verbosity, helper::LogMode::INFO);

    MPI_Group streamGroup;
    MPI_Group writerGroup;
    MPI_Comm readerComm;

    helper::HandshakeComm(m_Name, 'w', m_OpenTimeoutSecs, CommAsMPI(m_Comm),
                          streamGroup, writerGroup, m_ReaderGroup, m_StreamComm,
                          m_WriterComm, readerComm, m_Verbosity);

    m_WriterRank = m_Comm.Rank();
    m_WriterSize = m_Comm.Size();
    MPI_Comm_rank(m_StreamComm, &m_StreamRank);
    MPI_Comm_size(m_StreamComm, &m_StreamSize);

    int writerMasterStreamRank = -1;
    if (m_WriterRank == 0)
    {
        writerMasterStreamRank = m_StreamRank;
    }
    MPI_Allreduce(&writerMasterStreamRank, &m_WriterMasterStreamRank, 1,
                  MPI_INT, MPI_MAX, m_StreamComm);

    int readerMasterStreamRank = -1;
    MPI_Allreduce(&readerMasterStreamRank, &m_ReaderMasterStreamRank, 1,
                  MPI_INT, MPI_MAX, m_StreamComm);
}

void SscWriter::SyncWritePattern(bool finalStep)
{
    PERFSTUBS_SCOPED_TIMER_FUNC();

    helper::Log("Engine", "SSCWriter", "SyncWritePattern", "", 0, m_Comm.Rank(),
                5, m_Verbosity, helper::LogMode::INFO);

    ssc::Buffer localBuffer(8);
    localBuffer.value<uint64_t>() = 8;

    ssc::SerializeVariables(m_GlobalWritePattern[m_StreamRank], localBuffer,
                            m_StreamRank);

    if (m_WriterRank == 0)
    {
        ssc::SerializeAttributes(m_IO, localBuffer);
    }

    ssc::Buffer globalBuffer;

    ssc::AggregateMetadata(localBuffer, globalBuffer, m_WriterComm, finalStep,
                           m_WriterDefinitionsLocked);

    ssc::BroadcastMetadata(globalBuffer, m_WriterMasterStreamRank,
                           m_StreamComm);

    ssc::Deserialize(globalBuffer, m_GlobalWritePattern, m_IO, false, false);

    if (m_Verbosity >= 20 && m_WriterRank == 0)
    {
        ssc::PrintBlockVecVec(m_GlobalWritePattern, "Global Write Pattern");
    }
}

void SscWriter::SyncReadPattern()
{
    PERFSTUBS_SCOPED_TIMER_FUNC();

    helper::Log("Engine", "SSCWriter", "SyncReadPattern", "", 0, m_Comm.Rank(),
                5, m_Verbosity, helper::LogMode::INFO);

    ssc::Buffer globalBuffer;

    ssc::BroadcastMetadata(globalBuffer, m_ReaderMasterStreamRank,
                           m_StreamComm);

    m_ReaderSelectionsLocked = globalBuffer[1];

    ssc::Deserialize(globalBuffer, m_GlobalReadPattern, m_IO, false, false);
    m_AllSendingReaderRanks = ssc::CalculateOverlap(
        m_GlobalReadPattern, m_GlobalWritePattern[m_StreamRank]);
    CalculatePosition(m_GlobalWritePattern, m_GlobalReadPattern, m_WriterRank,
                      m_AllSendingReaderRanks);

    if (m_Verbosity >= 10)
    {
        for (int i = 0; i < m_WriterSize; ++i)
        {
            m_Comm.Barrier();
            if (i == m_WriterRank)
            {
                ssc::PrintRankPosMap(m_AllSendingReaderRanks,
                                     "Rank Pos Map for Writer " +
                                         std::to_string(m_WriterRank));
            }
        }
        m_Comm.Barrier();
    }
}

void SscWriter::CalculatePosition(ssc::BlockVecVec &writerVecVec,
                                  ssc::BlockVecVec &readerVecVec,
                                  const int writerRank,
                                  ssc::RankPosMap &allOverlapRanks)
{
    PERFSTUBS_SCOPED_TIMER_FUNC();
    for (auto &overlapRank : allOverlapRanks)
    {
        auto &readerRankMap = readerVecVec[overlapRank.first];
        auto currentReaderOverlapWriterRanks =
            CalculateOverlap(writerVecVec, readerRankMap);
        size_t bufferPosition = 0;
        for (int rank = 0; rank < static_cast<int>(writerVecVec.size()); ++rank)
        {
            bool hasOverlap = false;
            for (const auto r : currentReaderOverlapWriterRanks)
            {
                if (r.first == rank)
                {
                    hasOverlap = true;
                    break;
                }
            }
            if (hasOverlap)
            {
                currentReaderOverlapWriterRanks[rank].first = bufferPosition;
                auto &bv = writerVecVec[rank];
                size_t currentRankTotalSize = TotalDataSize(bv) + 1;
                currentReaderOverlapWriterRanks[rank].second =
                    currentRankTotalSize;
                bufferPosition += currentRankTotalSize;
            }
        }
        allOverlapRanks[overlapRank.first] =
            currentReaderOverlapWriterRanks[writerRank];
    }
}

#define declare_type(T)                                                        \
    void SscWriter::DoPutSync(Variable<T> &variable, const T *data)            \
    {                                                                          \
        helper::Log("Engine", "SSCWriter", "PutSync", variable.m_Name, 0,      \
                    m_Comm.Rank(), 5, m_Verbosity, helper::LogMode::INFO);     \
        PutDeferredCommon(variable, data);                                     \
        PerformPuts();                                                         \
    }                                                                          \
    void SscWriter::DoPutDeferred(Variable<T> &variable, const T *data)        \
    {                                                                          \
        helper::Log("Engine", "SSCWriter", "PutDeferred", variable.m_Name, 0,  \
                    m_Comm.Rank(), 5, m_Verbosity, helper::LogMode::INFO);     \
        PutDeferredCommon(variable, data);                                     \
    }
ADIOS2_FOREACH_STDTYPE_1ARG(declare_type)
#undef declare_type

void SscWriter::DoClose(const int transportIndex)
{
    PERFSTUBS_SCOPED_TIMER_FUNC();

    helper::Log("Engine", "SSCWriter", "Close", m_Name, 0, m_Comm.Rank(), 5,
                m_Verbosity, helper::LogMode::INFO);

    if (m_Threading && m_EndStepThread.joinable())
    {
        m_EndStepThread.join();
    }

    if (m_WriterDefinitionsLocked && m_ReaderSelectionsLocked)
    {
        if (m_CurrentStep > 0)
        {
            MPI_Waitall(static_cast<int>(m_MpiRequests.size()),
                        m_MpiRequests.data(), MPI_STATUSES_IGNORE);
            m_MpiRequests.clear();
        }

        m_Buffer[0] = 1;

        std::vector<MPI_Request> requests;
        for (const auto &i : m_AllSendingReaderRanks)
        {
            requests.emplace_back();
            MPI_Isend(m_Buffer.data(), 1, MPI_CHAR, i.first, 0, m_StreamComm,
                      &requests.back());
        }
        MPI_Waitall(static_cast<int>(requests.size()), requests.data(),
                    MPI_STATUS_IGNORE);
    }
    else
    {
        MPI_Win_free(&m_MpiWin);
        SyncWritePattern(true);
    }
}

} // end namespace engine
} // end namespace core
} // end namespace adios2
