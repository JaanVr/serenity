/*
 * Copyright (c) 2022, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibCore/Timer.h>
#include <LibGUI/Widget.h>
#include <LibGfx/Font.h>

namespace BrickStacker {

class Game final : public GUI::Widget {
    C_OBJECT(Game);

public:
    static int const s_side_length { 30 };
    static int const s_game_width { 10 * s_side_length };
    static int const s_game_height { 20 * s_side_length };
    static int const s_window_height { s_game_height };
    static int const s_window_width { s_game_width + 100 };

    virtual ~Game() override;
    void set_ghost(bool ghost_enabled);
    void set_paused(bool paused);

private:
    Game();

    virtual void paint_event(GUI::PaintEvent&) override;
    virtual void keydown_event(GUI::KeyEvent&) override;
    virtual void timer_event(Core::TimerEvent&) override;

    void tick();
    void reset();

    struct PlayfieldCoordinate {
        int const x, y;
        PlayfieldCoordinate(int x, int y)
            : x(x)
            , y(y)
        {
        }

        operator Gfx::Point<int>() const
        {
            return Gfx::Point<int>(x * s_side_length, y * s_side_length);
        }
    };

    struct PlayfieldSize {
        int const w, h;
        PlayfieldSize(int w, int h)
            : w(w)
            , h(h)
        {
        }

        operator Gfx::Size<int>() const
        {
            return Gfx::Size<int>(w * s_side_length, h * s_side_length);
        }
    };

    class Line {
    public:
        Line()
        {
            m_rectangle = Gfx::IntRect(0, s_game_height - s_side_length, s_game_width, s_side_length);
        }

        void move_up()
        {
            m_rectangle.set_y(m_rectangle.y() - s_side_length);
        }

        void reset()
        {
            m_rectangle.set_location({ 0, s_game_height - s_side_length });
        }

        Gfx::IntRect rectangle() const
        {
            return m_rectangle;
        }

    private:
        Gfx::IntRect m_rectangle;
    };

    Vector<Line> filled_lines();
    void clear_lines(Vector<Line> const& lines);

    class Transform {
    public:
        Transform(int dx, int dy, int dw, int dh)
            : m_coordinate(dx, dy)
            , m_size(dw, dh)
        {
        }
        Transform(PlayfieldCoordinate coordinate, PlayfieldSize size)
            : m_coordinate(coordinate)
            , m_size(size)
        {
        }
        Transform invert() const
        {
            return Transform(-1 * m_coordinate.x, -1 * m_coordinate.y, m_size.w, m_size.h);
        }

        PlayfieldCoordinate coordinate() const
        {
            return m_coordinate;
        }

        PlayfieldSize size() const
        {
            return m_size;
        }

    private:
        PlayfieldCoordinate const m_coordinate;
        PlayfieldSize const m_size;
    };

    template<typename T>
    class CircularBuffer {
    public:
        CircularBuffer(Vector<T> data)
            : m_data(move(data))
        {
        }
        size_t size() const { return m_data.size(); }
        T next()
        {
            m_current++;
            if (m_current >= size()) {
                m_current = 0;
            }
            return m_data[m_current];
        }
        T current()
        {
            return m_data[m_current];
        }
        T previous()
        {
            if (m_current == 0) {
                m_current = size() - 1;
            } else {
                m_current--;
            }
            return m_data[m_current];
        }

    private:
        size_t m_current = 0;
        Vector<T> m_data;
    };

    class Piece {
    public:
        Piece(Gfx::Color color, Vector<CircularBuffer<Transform>> transformations)
            : m_color(color)
            , m_transformations(move(transformations))
        {
        }

        Piece(Gfx::Color color)
            : m_color(color)
        {
        }
        void rotate_cw()
        {
            int i = 0;
            for (auto& t : m_transformations) {
                transform(t.next(), i);
                i++;
            }
        }
        void rotate_ccw()
        {
            // Construct a new transformation based on undoing the current coordinate change and changing the size to the previous transformations size
            int i = 0;
            for (auto& transformation : m_transformations) {
                auto current_transformation = transformation.current().invert();
                auto previous_transformation = transformation.previous();
                transform(Transform(current_transformation.coordinate(), previous_transformation.size()), i);
                i++;
            }
        }

        void transform(Transform const& transform, size_t index)
        {
            m_rectangles[index].set_location(m_rectangles[index].location() + transform.coordinate());
            m_rectangles[index].set_size(transform.size());
        }

        int top() const
        {
            int min = s_game_height + 1;
            for (auto r : m_rectangles) {
                if (min > r.top()) {
                    min = r.top();
                }
            }
            return min;
        }

        int bottom() const
        {
            int max = 0;
            for (auto r : m_rectangles) {
                if (max < r.bottom()) {
                    max = r.bottom();
                }
            }
            return max;
        }

        int left() const
        {
            int min = s_game_width + 1;
            for (auto r : m_rectangles) {
                if (min > r.left()) {
                    min = r.left();
                }
            }
            return min;
        }

        int right() const
        {
            int max = 0;
            for (auto r : m_rectangles) {
                if (max < r.right()) {
                    max = r.right();
                }
            }
            return max;
        }

        void move_up()
        {
            for (auto& r : m_rectangles) {
                r.set_y(r.y() - s_side_length);
            }
        }

        void move_down()
        {
            for (auto& r : m_rectangles) {
                r.set_y(r.y() + s_side_length);
            }
        }

        void move_left()
        {
            for (auto& r : m_rectangles) {
                r.set_x(r.x() - s_side_length);
            }
        }

        void move_right()
        {
            for (auto& r : m_rectangles) {
                r.set_x(r.x() + s_side_length);
            }
        }

        Vector<Gfx::IntRect> rects() const
        {
            return m_rectangles;
        }

        template<typename TUnaryPredicate>
        void remove_all_matching_rectangles(TUnaryPredicate predicate)
        {
            m_rectangles.remove_all_matching(predicate);
        }

        template<typename Callback>
        void for_each_rectangle(Callback callback)
        {
            for (auto& rect : m_rectangles) {
                callback(rect);
            }
        }

        template<typename Callback>
        void for_each_rectangle(Callback callback) const
        {
            for (auto const& rect : m_rectangles) {
                callback(rect);
            }
        }

        bool intersects(Piece const& tetromino) const
        {
            bool intersects = false;
            for (auto& re : m_rectangles) {
                tetromino.for_each_rectangle([&](auto const& r) {
                    if (r.intersects(re)) {
                        intersects = true;
                    }
                });
                if (intersects) {
                    return true;
                }
            }
            return false;
        }

        Gfx::Color color() const
        {
            return m_color;
        }

        bool is_empty() const
        {
            return m_rectangles.size() == 0;
        }

    protected: // FIXME: private:
        Gfx::Color const m_color;
        Vector<Gfx::IntRect> m_rectangles;
        Vector<CircularBuffer<Transform>> m_transformations;
    };

    class Z : public Piece {
        static constexpr Gfx::Color orange { 255, 165, 0 };

    public:
        Z()
            : Piece(orange, { CircularBuffer<Transform>({ Transform(-2, 1, 2, 1), Transform(2, -1, 1, 2) }), CircularBuffer<Transform>({ Transform(0, 1, 2, 1), Transform(0, -1, 1, 2) }) })
        {
            m_rectangles.append(Gfx::IntRect(PlayfieldCoordinate(3, 0), PlayfieldSize(2, 1)));
            m_rectangles.append(Gfx::IntRect(PlayfieldCoordinate(4, 1), PlayfieldSize(2, 1)));
        }
    };

    class I : public Piece {
    public:
        I()
            : Piece(Gfx::Color::Red, { CircularBuffer<Transform>({ Transform(-3, 2, 4, 1), Transform(3, -2, 1, 4) }) })
        {
            m_rectangles.append(Gfx::IntRect(PlayfieldCoordinate(3, 0), PlayfieldSize(4, 1)));
        }
    };

    class O : public Piece {
    public:
        O()
            : Piece(Gfx::Color::Cyan)
        {
            m_rectangles.append(Gfx::IntRect(PlayfieldCoordinate(4, 0), PlayfieldSize(2, 2)));
        }
    };

    class S : public Piece {
    public:
        S()
            : Piece(Gfx::Color::Yellow, { CircularBuffer<Transform>({ Transform(0, 1, 2, 1), Transform(0, -1, 1, 2) }), CircularBuffer<Transform>({ Transform(-2, 1, 2, 1), Transform(2, -1, 1, 2) }) })
        {
            m_rectangles.append(Gfx::IntRect(PlayfieldCoordinate(4, 0), PlayfieldSize(2, 1)));
            m_rectangles.append(Gfx::IntRect(PlayfieldCoordinate(3, 1), PlayfieldSize(2, 1)));
        }
    };

    class T : public Piece {
    public:
        T()
            : Piece(Gfx::Color::Green, { CircularBuffer<Transform>({ Transform(1, -1, 1, 1), Transform(1, 1, 1, 1), Transform(-1, 1, 1, 1), Transform(-1, -1, 1, 1) }), CircularBuffer<Transform>({ Transform(-1, 1, 3, 1), Transform(1, -1, 1, 3) }) })
        {
            m_rectangles.append(Gfx::IntRect(PlayfieldCoordinate(4, 0), PlayfieldSize(1, 1)));
            m_rectangles.append(Gfx::IntRect(PlayfieldCoordinate(3, 1), PlayfieldSize(3, 1)));
        }
    };

    class J : public Piece {
    public:
        J()
            : Piece(Gfx::Color::Magenta, { CircularBuffer<Transform>({ Transform(0, -2, 1, 1), Transform(2, 0, 1, 1), Transform(0, 2, 1, 1), Transform(-2, 0, 1, 1) }), CircularBuffer<Transform>({ Transform(-1, 1, 3, 1), Transform(1, -1, 1, 3) }) })
        {
            m_rectangles.append(Gfx::IntRect(PlayfieldCoordinate(3, 0), PlayfieldSize(1, 1)));
            m_rectangles.append(Gfx::IntRect(PlayfieldCoordinate(3, 1), PlayfieldSize(3, 1)));
        }
    };

    class L : public Piece {
    public:
        L()
            : Piece(Gfx::Color::Blue, { CircularBuffer<Transform>({ Transform(2, 0, 1, 1), Transform(0, 2, 1, 1), Transform(-2, 0, 1, 1), Transform(0, -2, 1, 1) }), CircularBuffer<Transform>({ Transform(-1, 1, 3, 1), Transform(1, -1, 1, 3) }) })
        {
            m_rectangles.append(Gfx::IntRect(PlayfieldCoordinate(5, 0), PlayfieldSize(1, 1)));
            m_rectangles.append(Gfx::IntRect(PlayfieldCoordinate(3, 1), PlayfieldSize(3, 1)));
        }
    };

    bool collision(Piece const& active_tetromino);
    Vector<Piece> m_pieces;
    Gfx::IntRect m_playfield { Gfx::IntRect(0, 0, s_game_width, s_game_height) };
    static Piece random_piece();
    bool m_debug_mode { false };
    bool m_ghost_piece_enabled { false };
    bool m_paused { false };
    size_t m_level { 0 };
    size_t m_lines_cleared { 0 };
    size_t m_total_lines_cleared { 0 };
    int m_score { 0 };
    RefPtr<Core::Timer> m_lock_delay_timer { Core::Timer::create_single_shot(500, [&]() { lock_piece(); }) };
    Array<int, 15> const m_timeouts {
        800,
        700,
        600,
        500,
        400,
        350,
        300,
        200,
        150,
        100,
        75,
        65,
        50,
        30,
        15
    };
    void increment_score(size_t const line_count);
    void lock_piece();
    void start_lock_delay();
};

}
