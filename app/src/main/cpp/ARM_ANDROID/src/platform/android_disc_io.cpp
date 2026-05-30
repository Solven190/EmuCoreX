// SPDX-License-Identifier: GPL-3.0+

#include "pcsx2/CDVD/CDVDdiscReader.h"

#include "common/Error.h"

#include <utility>

IOCtlSrc::IOCtlSrc(std::string filename)
	: m_filename(std::move(filename))
{
}

IOCtlSrc::~IOCtlSrc() = default;

bool IOCtlSrc::Reopen(Error* error)
{
	Error::SetString(error, "Physical optical disc devices are not exposed through Android's app sandbox.");
	return false;
}

u32 IOCtlSrc::GetSectorCount() const
{
	return 0;
}

const std::vector<toc_entry>& IOCtlSrc::ReadTOC() const
{
	return m_toc;
}

bool IOCtlSrc::ReadSectors2048(u32, u32, u8*) const
{
	return false;
}

bool IOCtlSrc::ReadSectors2352(u32, u32, u8*) const
{
	return false;
}

bool IOCtlSrc::ReadTrackSubQ(cdvdSubQ*) const
{
	return false;
}

u32 IOCtlSrc::GetLayerBreakAddress() const
{
	return 0;
}

s32 IOCtlSrc::GetMediaType() const
{
	return 0;
}

void IOCtlSrc::SetSpindleSpeed(bool) const
{
}

bool IOCtlSrc::DiscReady()
{
	return false;
}

bool IOCtlSrc::ReadDVDInfo()
{
	return false;
}

bool IOCtlSrc::ReadCDInfo()
{
	return false;
}

std::vector<std::string> GetOpticalDriveList()
{
	return {};
}

void GetValidDrive(std::string& drive)
{
	drive.clear();
}
