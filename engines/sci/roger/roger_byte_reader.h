/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SCI_ROGER_ROGER_BYTE_READER_H
#define SCI_ROGER_ROGER_BYTE_READER_H
#include "common/scummsys.h"
namespace Sci {
namespace Roger {

// Big-endian byte cursor over a pic bitstream. The SCI pic format is fully
// byte-aligned, so @4bitlabs/readers' MSB bit reader reduces to this.
class ByteReader {
public:
	ByteReader(const byte *data, uint32 size) : _data(data), _size(size), _pos(0) {}
	uint8 read8() { return _pos < _size ? _data[_pos++] : 0; }
	uint32 read24() {
		uint32 a = read8(), b = read8(), c = read8();
		return (a << 16) | (b << 8) | c;
	}
	int peek8() const { return _pos < _size ? (int)_data[_pos] : -1; }
	void skip(int bytes) { _pos += bytes; }
	bool eof() const { return _pos >= _size; }
private:
	const byte *_data;
	uint32 _size;
	uint32 _pos;
};

} // namespace Roger
} // namespace Sci
#endif
