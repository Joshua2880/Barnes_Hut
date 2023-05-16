#pragma once
#include <array>
#include <vector>
#include <Windows.h>

class QuadTree;

struct WndState
{
	BITMAPINFO bitmap_info;
	void* bitmap_memory;
	void *debug_bitmap_memory;
	QuadTree* state;
	bool running;
};

struct Vector
{
	friend Vector operator+(Vector p1, Vector p2) { return { p1.x + p2.x, p1.y + p2.y }; }
	friend Vector operator-(Vector p1, Vector p2) { return { p1.x - p2.x, p1.y - p2.y }; }
	friend Vector operator*(double s, Vector p) { return { s * p.x, s * p.y }; }
	friend Vector operator*(Vector p, double s) { return s * p; }
	friend Vector operator/(Vector p, double s) { return 1.0 / s * p; }

	Vector &operator+=(Vector other) { return *this = *this + other; }
	Vector& operator/=(double dividend) { return *this = *this / dividend; }

	double Magnitude() const { return sqrt(MagnitudeSquared()); }
	double MagnitudeSquared() const { return x * x + y * y; }

	Vector Normalise() const { return *this / Magnitude(); }

	double x, y;
};

struct Particle
{
	Vector pos{};
	Vector vel{};
	double mass{};
};

inline void DrawLine(void* pixels, size_t width, size_t height, Vector start, Vector end)
{
	Vector delta = end - start;
	double slope = delta.y / delta.x;
	size_t num_p = static_cast<size_t>(ceil(abs(abs(slope) <= 1 ? delta.x : delta.y)));
	Vector step = delta / static_cast<double>(num_p);
	for (size_t i = 0; i < num_p; ++i)
	{
		Vector p = start + i * step;
		if (static_cast<size_t>(p.x) < 0 || static_cast<size_t>(p.x) >= width ||
			static_cast<size_t>(p.y) < 0 || static_cast<size_t>(p.y) >= height)
		{
			continue;
		}
		static_cast<uint32_t*>(pixels)[static_cast<size_t>(p.y) * width + static_cast<size_t>(p.x)] = 0x00FFFFFF;
	}
}

inline void DrawLine(WndState& wnd_state, Vector start, Vector end)
{
	DrawLine(wnd_state.debug_bitmap_memory,
		wnd_state.bitmap_info.bmiHeader.biWidth,
		wnd_state.bitmap_info.bmiHeader.biHeight,
		start,
		end);
}

struct AABB
{
	std::array<AABB, 4> Partition() const
	{
		std::array<AABB, 4> result;
		result[0] = { l, t, l + width() / 2, t - height() / 2};
		result[1] = { l + width() / 2, t, l + width(), t - height() / 2};
		result[2] = { l, t - height() / 2, l + width() / 2, t - height()};
		result[3] = { l + width() / 2, t - height() / 2, l + width(), t - height()};
		return result;
	}

	bool Contains(Vector pos) const { return pos.x >= l & pos.x < r & pos.y >= b & pos.y < t; }

	double width() const { return r - l; }
	double height() const { return t - b; }

	void Draw(WndState& wnd_state, AABB world_space) const
	{
		double scaled_l = wnd_state.bitmap_info.bmiHeader.biWidth * (l - world_space.l) / world_space.width();
		double scaled_r = wnd_state.bitmap_info.bmiHeader.biWidth * (r - world_space.l) / world_space.width();
		double scaled_t = wnd_state.bitmap_info.bmiHeader.biHeight * (t - world_space.b) / world_space.height();
		double scaled_b = wnd_state.bitmap_info.bmiHeader.biHeight * (b - world_space.b) / world_space.height();
		DrawLine(wnd_state, { scaled_l, scaled_t}, { scaled_r, scaled_t});
		DrawLine(wnd_state, { scaled_l, scaled_t }, { scaled_l, scaled_b });
		DrawLine(wnd_state, { scaled_l, scaled_b }, { scaled_r, scaled_b });
		DrawLine(wnd_state, { scaled_r, scaled_t }, { scaled_r, scaled_b });
	}

	double l, t, r, b;
};

class QuadTree
{
public:
	QuadTree() = default;
	QuadTree(AABB bounds) : bounds_{bounds} {}
	QuadTree(AABB bounds, Particle *particle) : bounds_{ bounds }
	{
		insert(particle);
	}
	~QuadTree() { Destroy(); }

	void insert(Particle *particle)
	{
		if (!bounds_.Contains(particle->pos)) return;
		if (count_ == 0)
		{
			mass_ = particle->mass;
			centre_of_mass_ = particle->pos;
			particle_ = particle;
		}
		else
		{
			if (!north_west_) Split();
			north_west_->insert(particle);
			north_east_->insert(particle);
			south_west_->insert(particle);
			south_east_->insert(particle);
			centre_of_mass_ = (centre_of_mass_ * mass_ + particle->pos * particle->mass) / (mass_ + particle->mass);
			mass_ += particle->mass;
		}
		++count_;
	}

	void UpdateVelocities(QuadTree *root, double delta_time, double theta)
	{
		if (north_west_)
		{
			north_west_->UpdateVelocities(root, delta_time, theta);
			north_east_->UpdateVelocities(root, delta_time, theta);
			south_west_->UpdateVelocities(root, delta_time, theta);
			south_east_->UpdateVelocities(root, delta_time, theta);
		}
		else if (particle_)
		{
			root->UpdateVelocity(particle_, delta_time, theta);
		}
	}

	std::vector<Particle *> UpdatePositions(double delta_time)
	{
		std::vector<Particle*> particles;
		if (!count_) return particles;
		if (particle_)
		{
			centre_of_mass_ = particle_->pos += particle_->vel * delta_time;
			if (!bounds_.Contains(particle_->pos))
			{
				particles.emplace_back(particle_);
				mass_ = 0;
				centre_of_mass_ = { 0, 0 };
				particle_ = nullptr;
				--count_;
			}
			return particles;
		}
		std::vector<Particle*> temp = north_west_->UpdatePositions(delta_time);
		particles.insert(particles.end(), temp.begin(), temp.end());
		temp = north_east_->UpdatePositions(delta_time);
		particles.insert(particles.end(), temp.begin(), temp.end());
		temp = south_west_->UpdatePositions(delta_time);
		particles.insert(particles.end(), temp.begin(), temp.end());
		temp = south_east_->UpdatePositions(delta_time);
		particles.insert(particles.end(), temp.begin(), temp.end());

		for (auto iter = particles.begin(); iter != particles.end();)
		{
			if (!bounds_.Contains((*iter)->pos))
			{
				++iter;
				continue;
			}
			north_west_->insert(*iter);
			north_east_->insert(*iter);
			south_west_->insert(*iter);
			south_east_->insert(*iter);
			iter = particles.erase(iter);
		}

		mass_ = 0;
		centre_of_mass_ = { 0, 0 };

		centre_of_mass_ += north_west_->centre_of_mass_ * north_west_->mass_;
		mass_ += north_west_->mass_;
		centre_of_mass_ += north_east_->centre_of_mass_ * north_east_->mass_;
		mass_ += north_east_->mass_;
		centre_of_mass_ += south_west_->centre_of_mass_ * south_west_->mass_;
		mass_ += south_west_->mass_;
		centre_of_mass_ += south_east_->centre_of_mass_ * south_east_->mass_;
		mass_ += south_east_->mass_;

		centre_of_mass_ /= mass_;

		count_ -= particles.size();

		if (particles.size() > 4)
		{
			int i = 0;
		}
		if (abs(mass_) < std::numeric_limits<double>::epsilon())
		{
			centre_of_mass_ = { bounds_.l + bounds_.width() / 2, bounds_.b + bounds_.height() / 2 };
		}
		if (count_ <= 1)
		{
			Merge();
		}

		return particles;
	}

	std::vector<Particle *> ToVector()
	{
		std::vector<Particle*> particles{};
		if (!count_) return particles;
		if (particle_)
		{
			particles.emplace_back(particle_);
			return particles;
		}
		std::vector<Particle*> temp = north_west_->ToVector();
		particles.insert(particles.end(), temp.begin(), temp.end());
		temp = north_east_->ToVector();
		particles.insert(particles.end(), temp.begin(), temp.end());
		temp = south_west_->ToVector();
		particles.insert(particles.end(), temp.begin(), temp.end());
		temp = south_east_->ToVector();
		particles.insert(particles.end(), temp.begin(), temp.end());
		return particles;
	}

	void Draw(WndState &state, AABB world_space) const
	{
		if (north_west_)
		{
			north_west_->Draw(state, world_space);
			north_east_->Draw(state, world_space);
			south_west_->Draw(state, world_space);
			south_east_->Draw(state, world_space);
		}
		else
		{
			bounds_.Draw(state, world_space);
		}
	}

private:
	void Destroy()
	{
		if (!count_) return;
		if (north_west_)
		{
			delete north_west_;
			delete north_east_;
			delete south_west_;
			delete south_east_;
			north_west_ = nullptr;
			north_east_ = nullptr;
			south_west_ = nullptr;
			south_east_ = nullptr;
		}
		delete particle_;
		particle_ = nullptr;
	}

	void UpdateVelocity(Particle* particle, double delta_time, double theta)
	{
		if (particle == particle_ || !count_) return;
		double dist_sq = (particle->pos - centre_of_mass_).MagnitudeSquared();
		if (dist_sq < 0.000001) dist_sq = 0.000001;

		double constexpr p = 1.0;
		double constexpr z = 1.0;
		double constexpr c = 2.0;

		if (particle_)
		{
			particle->vel += delta_time * particle_->mass * (particle_->pos - particle->pos).Normalise() / dist_sq;
		}
		else if (bounds_.width() * bounds_.width() / dist_sq < theta * theta)
		{
			particle->vel += delta_time * mass_ * (centre_of_mass_ - particle->pos).Normalise() / dist_sq;
		}
		else
		{
			north_west_->UpdateVelocity(particle, delta_time, theta);
			north_east_->UpdateVelocity(particle, delta_time, theta);
			south_west_->UpdateVelocity(particle, delta_time, theta);
			south_east_->UpdateVelocity(particle, delta_time, theta);
		}
	}

	void Split()
	{
		std::array<AABB, 4> sub_quads = bounds_.Partition();
		north_west_ = new QuadTree(sub_quads[0], particle_);
		north_east_ = new QuadTree(sub_quads[1], particle_);
		south_west_ = new QuadTree(sub_quads[2], particle_);
		south_east_ = new QuadTree(sub_quads[3], particle_);
		particle_ = nullptr;
	}

	void Merge()
	{
		if (north_west_->count_)
		{
			particle_ = north_west_->particle_;
			north_west_->particle_ = nullptr;
		}
		else if (north_east_->count_)
		{
			particle_ = north_east_->particle_;
			north_east_->particle_ = nullptr;
		}
		else if (south_west_->count_)
		{
			particle_ = south_west_->particle_;
			south_west_->particle_ = nullptr;
		}
		else if (south_east_->count_)
		{
			particle_ = south_east_->particle_;
			south_east_->particle_ = nullptr;
		}
		delete north_west_;
		delete north_east_;
		delete south_west_;
		delete south_east_;
		north_west_ = nullptr;
		north_east_ = nullptr;
		south_west_ = nullptr;
		south_east_ = nullptr;
	}

	Particle *particle_{};

	QuadTree *north_west_{};
	QuadTree *north_east_{};
	QuadTree *south_west_{};
	QuadTree *south_east_{};

	size_t count_{};

	double mass_{};
	Vector centre_of_mass_{};

	AABB bounds_{};
};

