// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/***************************************************************************

    drivenum.cpp

    Driver enumeration helpers.

***************************************************************************/

#include "emu.h"
#include "drivenum.h"
#include "softlist_dev.h"

#include <ctype.h>



//**************************************************************************
//  DRIVER LIST
//**************************************************************************

bool                            driver_list::s_has_sorted = false;
std::mutex                      driver_list::s_sort_mutex;
driver_list::game_driver_vector driver_list::s_drivers_sorted;

//-------------------------------------------------
//  find - find a driver by name
//-------------------------------------------------

int driver_list::find(const char *name)
{
	// if no name, bail
	if (!name)
		return -1;

	// binary search to find it
	game_driver_vector const &drivers(drivers_sorted());
	auto const cmp = [] (game_driver const *driver, char const *name) { return core_stricmp(driver->name, name) < 0; };
	auto const result = std::lower_bound(drivers.begin(), drivers.end(), name, cmp);
	return ((result == drivers.end()) || core_stricmp((*result)->name, name)) ? -1 : std::distance(drivers.begin(), result);
}


//-------------------------------------------------
//  matches - true if we match, taking into
//  account wildcards in the wildstring
//-------------------------------------------------

bool driver_list::matches(const char *wildstring, const char *string)
{
	// can only match internal drivers if the wildstring starts with an underscore
	if (string[0] == '_' && (wildstring == nullptr || wildstring[0] != '_'))
		return false;

	// match everything else normally
	return (wildstring == nullptr || core_strwildcmp(wildstring, string) == 0);
}


//-------------------------------------------------
//  penalty_compare - compare two strings for
//  closeness and assign a score.
//-------------------------------------------------

int driver_list::penalty_compare(const char *source, const char *target)
{
	int gaps = 1;
	bool last = true;

	// scan the strings
	for ( ; *source && *target; target++)
	{
		// do a case insensitive match
		bool const match(tolower(u8(*source)) == tolower(u8(*target)));

		// if we matched, advance the source
		if (match)
			source++;

		// if the match state changed, count gaps
		if (match != last)
		{
			last = match;
			if (!match)
				gaps++;
		}
	}

	// penalty if short string does not completely fit in
	for ( ; *source; source++)
		gaps++;

	// if we matched perfectly, gaps == 0
	if (gaps == 1 && *source == 0 && *target == 0)
		gaps = 0;

	return gaps;
}


driver_list::game_driver_vector const &driver_list::drivers_sorted()
{
	// Wild DCLP appeared!
	if (!s_has_sorted)
	{
		std::atomic_thread_fence(std::memory_order_acquire);
		std::lock_guard<std::mutex> lock(s_sort_mutex);
		if (!s_has_sorted)
		{
			assert(s_drivers_sorted.empty());
			s_drivers_sorted = std::move(drivers_unsorted());
			auto const cmp = [] (game_driver const *x, game_driver const *y) { return core_stricmp(x->name, y->name) < 0; };
			std::sort(s_drivers_sorted.begin(), s_drivers_sorted.end(), cmp);
			s_has_sorted = true;
		}
		std::atomic_thread_fence(std::memory_order_release);
	}
	return s_drivers_sorted;
}


driver_list::game_driver_vector &driver_list::drivers_unsorted()
{
	// This is necessary because we can't depend on static initialisation order across compilation units.
	// It's very much not thread-safe, but we can get away with it because it's called by static driver
	// registrars before main(), and hence before the program can go multi-threaded.
	static std::unique_ptr<game_driver_vector> drivers(new game_driver_vector);
	return *drivers;
}



//**************************************************************************
//  DRIVER ENUMERATOR
//**************************************************************************

//-------------------------------------------------
//  driver_enumerator - constructor
//-------------------------------------------------

driver_enumerator::driver_enumerator(emu_options &options)
	: m_current(-1)
	, m_filtered_count(0)
	, m_options(options)
	, m_included(m_drivers.size())
	, m_config(CONFIG_CACHE_COUNT)
{
	include_all();
}


driver_enumerator::driver_enumerator(emu_options &options, const char *string)
	: driver_enumerator(options)
{
	filter(string);
}


driver_enumerator::driver_enumerator(emu_options &options, const game_driver &driver)
	: driver_enumerator(options)
{
	filter(driver);
}


//-------------------------------------------------
//  ~driver_enumerator - destructor
//-------------------------------------------------

driver_enumerator::~driver_enumerator()
{
	// configs are freed by the cache
}


//-------------------------------------------------
//  config - return a machine_config for the given
//  driver, allocating on demand if needed
//-------------------------------------------------

std::shared_ptr<machine_config> const &driver_enumerator::config(std::size_t index, emu_options &options) const
{
	assert(index < m_drivers.size());

	// if we don't have it cached, add it
	std::shared_ptr<machine_config> &config = m_config[index];
	if (!config)
		config = std::make_shared<machine_config>(*m_drivers[index], options);

	return config;
}


//-------------------------------------------------
//  filter - filter the driver list against the
//  given string
//-------------------------------------------------

std::size_t driver_enumerator::filter(const char *filterstring)
{
	// reset the count
	exclude_all();

	// match name against each driver in the list
	for (std::size_t index = 0; index < m_drivers.size(); index++)
		if (matches(filterstring, m_drivers[index]->name))
			include(index);

	return m_filtered_count;
}


//-------------------------------------------------
//  filter - filter the driver list against the
//  given driver
//-------------------------------------------------

std::size_t driver_enumerator::filter(const game_driver &driver)
{
	// reset the count
	exclude_all();

	// match name against each driver in the list
	for (std::size_t index = 0; index < m_drivers.size(); index++)
		if (m_drivers[index] == &driver)
			include(index);

	return m_filtered_count;
}


//-------------------------------------------------
//  include_all - include all non-internal drivers
//-------------------------------------------------

void driver_enumerator::include_all()
{
	std::fill(m_included.begin(), m_included.end(), true);
	m_filtered_count = m_included.size();

	// always exclude the empty driver
	exclude(find("___empty"));
}


//-------------------------------------------------
//  next - get the next driver matching the given
//  filter
//-------------------------------------------------

bool driver_enumerator::next()
{
	release_current();

	// always advance one
	// if we have a filter, scan forward to the next match
	for (m_current++; (m_current < m_drivers.size()) && !m_included[m_current]; m_current++) { }

	// return true if we end up in range
	return (m_current >= 0) && (m_current < m_drivers.size());
}


//-------------------------------------------------
//  next_excluded - get the next driver that is
//  not currently included in the list
//-------------------------------------------------

bool driver_enumerator::next_excluded()
{
	release_current();

	// always advance one
	// if we have a filter, scan forward to the next match
	for (m_current++; (m_current < m_drivers.size()) && m_included[m_current]; m_current++) { }

	// return true if we end up in range
	return (m_current >= 0) && (m_current < m_drivers.size());
}


//-------------------------------------------------
//  driver_sort_callback - compare two items in
//  an array of game_driver pointers
//-------------------------------------------------

void driver_enumerator::find_approximate_matches(const char *string, std::size_t count, int *results)
{
#undef rand

	// if no name, pick random entries
	if (!string || !string[0])
	{
		// seed the RNG first
		srand(osd_ticks());

		// allocate a temporary list
		std::vector<int> templist(m_filtered_count);
		int arrayindex = 0;
		for (int index = 0; index < m_drivers.size(); index++)
			if (m_included[index])
				templist[arrayindex++] = index;
		assert(arrayindex == m_filtered_count);

		// shuffle
		for (int shufnum = 0; shufnum < (4 * m_drivers.size()); shufnum++)
		{
			int item1 = rand() % m_filtered_count;
			int item2 = rand() % m_filtered_count;
			int temp = templist[item1];
			templist[item1] = templist[item2];
			templist[item2] = temp;
		}

		// copy out the first few entries
		for (int matchnum = 0; matchnum < count; matchnum++)
			results[matchnum] = templist[matchnum % m_filtered_count];
	}
	else
	{
		// allocate memory to track the penalty value
		std::vector<int> penalty(count);

		// initialize everyone's states
		for (int matchnum = 0; matchnum < count; matchnum++)
		{
			penalty[matchnum] = 9999;
			results[matchnum] = -1;
		}

		// scan the entire drivers array
		for (int index = 0; index < m_drivers.size(); index++)
		{
			// skip things that can't run
			if (m_included[index] &&  !(m_drivers[index]->flags & MACHINE_NO_STANDALONE))
			{
				// pick the best match between driver name and description
				int curpenalty = penalty_compare(string, m_drivers[index]->description);
				int tmp = penalty_compare(string, m_drivers[index]->name);
				curpenalty = (std::min)(curpenalty, tmp);

				// insert into the sorted table of matches
				for (int matchnum = count - 1; matchnum >= 0; matchnum--)
				{
					// stop if we're worse than the current entry
					if (curpenalty >= penalty[matchnum])
						break;

					// as long as this isn't the last entry, bump this one down
					if (matchnum < count - 1)
					{
						penalty[matchnum + 1] = penalty[matchnum];
						results[matchnum + 1] = results[matchnum];
					}
					results[matchnum] = index;
					penalty[matchnum] = curpenalty;
				}
			}
		}
	}
}


//-------------------------------------------------
//  release_current - release bulky memory
//  structures from the current entry because
//  we're done with it
//-------------------------------------------------

void driver_enumerator::release_current() const
{
	// skip if no current entry
	if ((m_current >= 0) && (m_current < m_drivers.size()))
	{
		// skip if we haven't cached a config
		auto const cached = m_config.find(m_current);
		if (cached != m_config.end())
		{
			// iterate over software lists in this entry and reset
			for (software_list_device &swlistdev : software_list_device_iterator(cached->second->root_device()))
				swlistdev.release();
		}
	}
}


game_driver_registrar::game_driver_registrar(game_driver const &driver)
{
	driver_list::drivers_unsorted().push_back(&driver);
}
