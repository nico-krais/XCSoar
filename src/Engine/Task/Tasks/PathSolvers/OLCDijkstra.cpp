/* Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009

	M Roberts (original release)
	Robin Birch <robinb@ruffnready.co.uk>
	Samuel Gisiger <samuel.gisiger@triadis.ch>
	Jeff Goodenough <jeff@enborne.f2s.com>
	Alastair Harrison <aharrison@magic.force9.co.uk>
	Scott Penrose <scottp@dd.com.au>
	John Wharington <jwharington@gmail.com>
	Lars H <lars_hn@hotmail.com>
	Rob Dunning <rob@raspberryridgesheepfarm.com>
	Russell King <rmk@arm.linux.org.uk>
	Paolo Ventafridda <coolwind@email.it>
	Tobias Lohner <tobias@lohner-net.de>
	Mirek Jezek <mjezek@ipplc.cz>
	Max Kellermann <max@duempel.org>
	Tobias Bieniek <tobias.bieniek@gmx.de>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "OLCDijkstra.hpp"
#include "Task/Tasks/OnlineContest.hpp"
#include <assert.h>


OLCDijkstra::~OLCDijkstra() {
}


OLCDijkstra::OLCDijkstra(OnlineContest& _olc, 
                         const unsigned n_legs,
                         const unsigned finish_alt_diff):
  NavDijkstra<TracePoint>(n_legs+1),
  olc(_olc),
  m_finish_alt_diff(finish_alt_diff),
  m_dijkstra(ScanTaskPoint(0,0), false)
{
  m_weightings.reserve(n_legs);
  m_dijkstra.clear();  // start with empty
}

void
OLCDijkstra::set_weightings()
{
  m_weightings.clear();
  for (unsigned i=0; i+1<num_stages; ++i) {
    m_weightings.push_back(5);
  }
}

const TracePoint &
OLCDijkstra::get_point(const ScanTaskPoint &sp) const
{
  assert(sp.second<n_points);
  return olc.get_trace_points()[sp.second];
}


bool 
OLCDijkstra::solve()
{
  if (m_dijkstra.empty()) {

    set_weightings();

    n_points = olc.get_trace_points().size();
  }

  if (n_points<num_stages) {
    return false;
  }

  if (m_dijkstra.empty()) {
    const ScanTaskPoint start(0,0);
    m_dijkstra.reset(start);
    add_start_edges(m_dijkstra);
  }

  return distance_general(m_dijkstra);

}

fixed
OLCDijkstra::score(fixed& the_distance) 
{
  if (solve()) {
    fixed dist = fixed_zero;
    for (unsigned i=0; i+1<num_stages; ++i) {
      dist += m_weightings[0]*solution[i].distance(solution[i+1].get_location());
    }
    static const fixed fixed_fifth(0.2);
    dist *= fixed_fifth;
    the_distance = dist;
    return dist;
  }
  return fixed_zero;
}

unsigned 
OLCDijkstra::stage_end(const ScanTaskPoint &sp) const
{
  return (int)n_points+sp.first-num_stages+1;
}

void 
OLCDijkstra::add_edges(DijkstraTaskPoint &dijkstra,
                       const ScanTaskPoint& origin) 
{
  ScanTaskPoint destination(origin.first+1, origin.second+1);
  const unsigned end = stage_end(destination);

  find_solution(dijkstra, origin);
  
  for (; destination.second!= end; ++destination.second) {
    if (admit_candidate(destination)) {
      const unsigned d = m_weightings[origin.first]*distance(origin, destination);
      dijkstra.link(destination, origin, d);
    }
  }
}


void
OLCDijkstra::add_start_edges(DijkstraTaskPoint &dijkstra)
{
  dijkstra.pop();

  ScanTaskPoint destination(0,0);
  const unsigned end = stage_end(destination);
  
  for (; destination.second!= end; ++destination.second) {
    dijkstra.link(destination, destination, 0);
  }
}


bool
OLCDijkstra::admit_candidate(const ScanTaskPoint &candidate) const
{
  if (!is_final(candidate)) 
    return true;
  else {
    return (get_point(candidate).altitude+m_finish_alt_diff >= 
            solution[0].altitude);
  }
}

bool 
OLCDijkstra::finish_satisfied(const ScanTaskPoint &sp) const
{
  if (admit_candidate(sp)) {
    return true;
  } else {
    return false;
  }
}


void
OLCDijkstra::copy_solution(TracePointVector &vec)
{
  vec.clear();
  vec.reserve(num_stages);
  for (unsigned i=0; i<num_stages; i++) {
    vec.push_back(solution[i]);
  }
}


/*

OLC classic:
- start, 5 turnpoints, finish
- weightings: 1,1,1,0.8,0.8,0.6

FAI OLC:
- start, 2 turnpoints, finish
- if d<500km, shortest leg >= 28% d; else shortest leg >= 25%d
- considered closed if a fix is within 1km of starting point
- weightings: 1 all

OLC classic + FAI-OLC
- min finish alt is 1000m below start altitude 
- start altitude is lowest altitude before reaching start
- start time is time at which start altitude is reached
- The finish altitude is the highest altitude after reaching the finish point and before end of free flight.
- The finish time is the time at which the finish altitude is reached after the finish point is reached.


OLC league:
- start, 3 turnpoints, finish
- weightings: 1 all
- The sprint start point can not be higher than the sprint end point.
- The sprint start altitude is the altitude at the sprint start point.
- Sprint arrival height is the altitude at the sprint end point.
- The average speed (points) of each individual flight is the sum of
the distances from sprint start, around up to three turnpoints, to the
sprint end divided DAeC index increased by 100, multiplied by 200 and
divided by 2.5h: [formula: Points = km / 2,5 * 200 / (Index+100)

*/
