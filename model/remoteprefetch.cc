/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2016 Technische Universitaet Berlin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "remote.h"
#include "httplib.h"
#include "json.hpp"
#include <cassert>


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RemotePrefetchAlgorithm");

NS_OBJECT_ENSURE_REGISTERED (RemotePrefetchAlgorithm);

RemotePrefetchAlgorithm::RemotePrefetchAlgorithm (  const videoData &videoData,
                                  const playbackData & playbackData,
                                  const bufferData & bufferData,
                                  const throughputData & throughput,
								  const uint32_t simulationId,
								  std::string fifoPath,
								  int abrPort
								  ) :
  AdaptationAlgorithm (videoData, playbackData, bufferData, throughput),
	selectedBitrateIndex(),
	cookie(),
	simulationId(simulationId),
	 fifoPath(fifoPath),
	 abrPort(abrPort)
{
  NS_LOG_INFO (this);
}

#define MIN(x, y) (x) < (y) ? (x) : (y)

algorithmReply
RemotePrefetchAlgorithm::GetNextRep (const int64_t segmentCounter, int64_t clientId)
{
	const int64_t timeNow = Simulator::Now ().GetMicroSeconds ();


	nlohmann::json stat;
	nlohmann::json vidData = {
		{"segSizes",  m_videoData.segmentSize},
		{"bitrates", m_videoData.averageBitrate},
		{"segDur", m_videoData.segmentDuration},
		{"simId", simulationId},
	};
// 	stat["video"] = vidData;

	int64_t playbacktime = 0;
	int64_t availBuf = 0;
	int64_t lastBitrateIndex = 0;
	int64_t totalStall = 0;
	int64_t lastFinishedAt = 0;
	int64_t lastStartedAt = 0;
	int64_t lastClen = 0;
	if(segmentCounter > 0) {
		playbacktime = m_playbackData.playbackStart.back() + std::min(m_videoData.segmentDuration, timeNow - m_playbackData.playbackStart.back());
		availBuf = std::max(0l, m_bufferData.bufferLevelNew.back() - playbacktime);
		lastBitrateIndex = selectedBitrateIndex.back();
		totalStall = timeNow - playbacktime + m_playbackData.playbackStart.front();
		assert(totalStall >= 0);

		lastFinishedAt = m_throughput.transmissionEnd.back();
		lastStartedAt = m_throughput.transmissionStart.back();
		lastClen = m_throughput.bytesReceived.back();
	}

	int64_t lastBitrate = m_videoData.averageBitrate.at(lastBitrateIndex);

	nlohmann::json playback = {
		{"maxPlaybackBuffer", 5*m_videoData.segmentDuration},
		{"playbacktime", playbacktime},
		{"bufferUpto", m_videoData.segmentDuration * segmentCounter},
		{"availBuf", availBuf},
		{"lastBitrate", lastBitrate},
		{"lastBitrateIndex", lastBitrateIndex},
		{"totalStall", totalStall},
		{"lastFinishedAt", lastFinishedAt},
		{"lastStartedAt", lastStartedAt},
		{"lastClen", lastClen},
		{"segmentId", segmentCounter}
	};
	stat["playback"] = playback;
	stat["playerId"] = clientId;
	if(cookie.empty())
		stat["video"] = vidData;
	else
		stat["cookie"] = cookie;

	nlohmann::json response;
	if(fifoPath.empty()){
		httplib::Client cli("localhost", abrPort);
		auto res = cli.Post("/large-data", stat.dump(), "text/plain");

		response = nlohmann::json::parse(res->body);
	}
	else{
		std::ofstream o(fifoPath);
		o << stat;
		o.close();
		std::ifstream i(fifoPath);
		i >> response;
		i.close();
	}
// 	std::cout << response;

	if(cookie.empty()){
		cookie = response["cookie"].get<std::string>();
	}

	int nextBitrateIndex = response["bitrate"].get<int>();
	int64_t delay = response["delay"].get<int64_t>();

	int64_t prefetchTime       = response["prefetchTime"].get<int64_t>();
	int64_t prefetchIndex      = response["prefetchIndex"].get<int64_t>();
	int64_t prefetchRepIndex   = response["prefetchRepIndex"].get<int64_t>();

	algorithmReply ar = { nextBitrateIndex, delay, timeNow, 0, 0, prefetchTime, prefetchIndex, prefetchRepIndex };
	selectedBitrateIndex.push_back(nextBitrateIndex);
	return ar;
}


} // namespace ns3
