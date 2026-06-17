#include "media/tile_sequencer.hpp"

#include "core/base64.hpp"
#include "media/decode.hpp"
#include "net/json.hpp"

#include <sstream>

namespace metaagent::media {
namespace {

int32_t clamp_grid_count(int32_t value)
{
	if (value < 1)
	{
		return 1;
	}
	if (value > 32)
	{
		return 32;
	}
	return value;
}

core::String make_signal_id()
{
	static int32_t counter = 0;
	return "seq-" + std::to_string(++counter);
}

} // namespace

core::String tile_sequence_state_name(const TileSequenceState state)
{
	switch (state)
	{
	case TileSequenceState::Idle: return "idle";
	case TileSequenceState::Ready: return "ready";
	case TileSequenceState::Playing: return "playing";
	case TileSequenceState::Paused: return "paused";
	case TileSequenceState::Complete: return "complete";
	default: return "unknown";
	}
}

core::String build_tile_sequence_status_json(const TileSequenceStatus& status)
{
	std::ostringstream stream;
	stream << '{';
	stream << net::json_string_field("state", tile_sequence_state_name(status.state)) << ',';
	stream << net::json_string_field("source_path", status.source_path) << ',';
	stream << "\"columns\":" << status.columns << ',';
	stream << "\"rows\":" << status.rows << ',';
	stream << "\"current_index\":" << status.current_index << ',';
	stream << "\"total_tiles\":" << status.total_tiles << ',';
	stream << "\"tile_width\":" << status.tile_width << ',';
	stream << "\"tile_height\":" << status.tile_height << ',';
	stream << "\"hold_seconds\":" << status.hold_seconds << ',';
	stream << net::json_string_field("error", status.error_message);
	stream << '}';
	return stream.str();
}

bool TileSequencer::load_image(const core::String& path, int32_t columns, int32_t rows)
{
	status_ = TileSequenceStatus {};
	elapsed_seconds_ = 0.0f;
	source_image_ = RgbaImage {};

	columns = clamp_grid_count(columns);
	rows = clamp_grid_count(rows);

	RgbaImage decoded;
	if (!decode_image_from_file(path, decoded) || !decoded.valid())
	{
		status_.state = TileSequenceState::Idle;
		status_.error_message = "Failed to decode image: " + path;
		return false;
	}

	source_image_ = std::move(decoded);
	status_.source_path = path;
	status_.columns = columns;
	status_.rows = rows;
	status_.total_tiles = columns * rows;
	status_.current_index = 0;
	status_.tile_width = source_image_.width / columns;
	status_.tile_height = source_image_.height / rows;

	if (status_.tile_width < 1 || status_.tile_height < 1)
	{
		status_.state = TileSequenceState::Idle;
		status_.error_message = "Image is too small for the requested grid.";
		source_image_ = RgbaImage {};
		return false;
	}

	status_.state = TileSequenceState::Ready;
	return true;
}

void TileSequencer::reset()
{
	if (source_image_.valid())
	{
		status_.current_index = 0;
		status_.state = TileSequenceState::Ready;
		status_.error_message.clear();
	}
	else
	{
		status_ = TileSequenceStatus {};
	}
	elapsed_seconds_ = 0.0f;
}

bool TileSequencer::step_forward()
{
	if (!source_image_.valid() || status_.total_tiles <= 0)
	{
		status_.error_message = "No sequence loaded.";
		return false;
	}

	if (status_.current_index >= status_.total_tiles)
	{
		status_.state = TileSequenceState::Complete;
		return false;
	}

	++status_.current_index;
	if (status_.current_index >= status_.total_tiles)
	{
		status_.state = TileSequenceState::Complete;
	}
	else if (status_.state != TileSequenceState::Playing)
	{
		status_.state = TileSequenceState::Ready;
	}

	status_.error_message.clear();
	return true;
}

void TileSequencer::play()
{
	if (!source_image_.valid())
	{
		status_.error_message = "No sequence loaded.";
		return;
	}

	if (status_.state == TileSequenceState::Complete)
	{
		status_.current_index = 0;
	}

	status_.state = TileSequenceState::Playing;
	status_.error_message.clear();
	elapsed_seconds_ = 0.0f;
}

void TileSequencer::pause()
{
	if (status_.state == TileSequenceState::Playing)
	{
		status_.state = TileSequenceState::Paused;
	}
}

void TileSequencer::tick(const float delta_seconds)
{
	if (status_.state != TileSequenceState::Playing)
	{
		return;
	}

	elapsed_seconds_ += delta_seconds;
	if (elapsed_seconds_ < status_.hold_seconds)
	{
		return;
	}

	elapsed_seconds_ = 0.0f;
	step_forward();
}

TileSequenceStatus TileSequencer::status() const
{
	return status_;
}

bool TileSequencer::extract_tile_rgba(
	const int32_t tile_index,
	core::Array<uint8_t>& out_rgba,
	int32_t& out_width,
	int32_t& out_height) const
{
	out_width = 0;
	out_height = 0;
	if (!source_image_.valid() || tile_index < 0 || tile_index >= status_.total_tiles)
	{
		return false;
	}

	const int32_t column = tile_index % status_.columns;
	const int32_t row = tile_index / status_.columns;
	const int32_t origin_x = column * status_.tile_width;
	const int32_t origin_y = row * status_.tile_height;

	constexpr int32_t kMaxTransportDimension = 128;
	int32_t sample_width = status_.tile_width;
	int32_t sample_height = status_.tile_height;
	if (sample_width > kMaxTransportDimension || sample_height > kMaxTransportDimension)
	{
		sample_width = kMaxTransportDimension;
		sample_height = kMaxTransportDimension;
	}

	out_rgba.clear();
	out_rgba.reserve(static_cast<size_t>(sample_width * sample_height * 4));

	for (int32_t y = 0; y < sample_height; ++y)
	{
		const int32_t source_y = origin_y
			+ (y * status_.tile_height) / sample_height;
		for (int32_t x = 0; x < sample_width; ++x)
		{
			const int32_t source_x = origin_x
				+ (x * status_.tile_width) / sample_width;
			const size_t source_index =
				static_cast<size_t>(source_y * source_image_.width + source_x);
			if (source_index >= source_image_.pixels.size())
			{
				return false;
			}

			const core::ColorRGBA& color = source_image_.pixels[source_index];
			out_rgba.push_back(color.r);
			out_rgba.push_back(color.g);
			out_rgba.push_back(color.b);
			out_rgba.push_back(color.a);
		}
	}

	out_width = sample_width;
	out_height = sample_height;
	return true;
}

net::SignalEnvelope TileSequencer::build_tile_signal(const core::String& target_id) const
{
	net::SignalEnvelope envelope;
	envelope.type = "tile";
	envelope.target = target_id;
	envelope.id = make_signal_id();

	const int32_t tile_index = status_.current_index;
	core::Array<uint8_t> rgba;
	int32_t transport_width = 0;
	int32_t transport_height = 0;
	if (!extract_tile_rgba(tile_index, rgba, transport_width, transport_height))
	{
		envelope.payload_json = "{"
			+ net::json_string_field("error", "Unable to extract tile pixels")
			+ "}";
		return envelope;
	}

	const int32_t column = (status_.columns > 0) ? (tile_index % status_.columns) : 0;
	const int32_t row = (status_.columns > 0) ? (tile_index / status_.columns) : 0;

	std::ostringstream payload;
	payload << '{';
	payload << net::json_string_field("seq_id", status_.source_path) << ',';
	payload << "\"index\":" << tile_index << ',';
	payload << "\"columns\":" << status_.columns << ',';
	payload << "\"rows\":" << status_.rows << ',';
	payload << "\"column\":" << column << ',';
	payload << "\"row\":" << row << ',';
	payload << "\"width\":" << transport_width << ',';
	payload << "\"height\":" << transport_height << ',';
	payload << "\"source_tile_width\":" << status_.tile_width << ',';
	payload << "\"source_tile_height\":" << status_.tile_height << ',';
	payload << net::json_string_field("encoding", "rgba_base64") << ',';
	payload << net::json_string_field("data", core::base64_encode(rgba));
	payload << '}';

	envelope.payload_json = payload.str();
	return envelope;
}

} // namespace metaagent::media
