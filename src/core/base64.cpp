#include "core/base64.hpp"

namespace metaagent::core {
namespace {

constexpr char kAlphabet[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

} // namespace

core::String base64_encode(const uint8_t* data, const size_t size)
{
	if (data == nullptr || size == 0)
	{
		return {};
	}

	core::String encoded;
	encoded.reserve(((size + 2) / 3) * 4);

	for (size_t index = 0; index < size; index += 3)
	{
		const uint32_t byte0 = data[index];
		const uint32_t byte1 = (index + 1 < size) ? data[index + 1] : 0;
		const uint32_t byte2 = (index + 2 < size) ? data[index + 2] : 0;
		const uint32_t triple = (byte0 << 16) | (byte1 << 8) | byte2;

		encoded.push_back(kAlphabet[(triple >> 18) & 0x3F]);
		encoded.push_back(kAlphabet[(triple >> 12) & 0x3F]);
		encoded.push_back((index + 1 < size) ? kAlphabet[(triple >> 6) & 0x3F] : '=');
		encoded.push_back((index + 2 < size) ? kAlphabet[triple & 0x3F] : '=');
	}

	return encoded;
}

core::String base64_encode(const core::Array<uint8_t>& data)
{
	return base64_encode(data.data(), data.size());
}

} // namespace metaagent::core
