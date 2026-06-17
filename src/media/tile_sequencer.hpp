#pragma once

#include "core/types.hpp"
#include "export.hpp"
#include "media/image.hpp"
#include "net/signal_types.hpp"

namespace metaagent::media {

enum class TileSequenceState {
	Idle,
	Ready,
	Playing,
	Paused,
	Complete,
};

struct TileSequenceStatus {
	TileSequenceState state = TileSequenceState::Idle;
	core::String source_path;
	int32_t columns = 0;
	int32_t rows = 0;
	int32_t current_index = 0;
	int32_t total_tiles = 0;
	int32_t tile_width = 0;
	int32_t tile_height = 0;
	float hold_seconds = 0.5f;
	core::String error_message;
};

class TileSequencer {
public:
	bool load_image(const core::String& path, int32_t columns, int32_t rows);
	void reset();
	bool step_forward();
	void play();
	void pause();
	void tick(float delta_seconds);

	TileSequenceStatus status() const;
	net::SignalEnvelope build_tile_signal(const core::String& target_id) const;

private:
	bool extract_tile_rgba(int32_t tile_index, core::Array<uint8_t>& out_rgba, int32_t& out_width, int32_t& out_height) const;

	RgbaImage source_image_;
	TileSequenceStatus status_;
	float elapsed_seconds_ = 0.0f;
};

METAAGENT_API core::String tile_sequence_state_name(TileSequenceState state);

METAAGENT_API core::String build_tile_sequence_status_json(const TileSequenceStatus& status);

} // namespace metaagent::media
