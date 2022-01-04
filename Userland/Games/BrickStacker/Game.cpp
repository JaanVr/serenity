/*
 * Copyright (c) 2022, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Game.h"
#include <AK/Random.h>
#include <LibGUI/Application.h>
#include <LibGUI/MessageBox.h>
#include <LibGUI/Painter.h>

namespace BrickStacker {

Game::Game()
{
    reset();
    start_timer(m_timeouts[m_level]);
}

Game::~Game()
{
}

void Game::timer_event(Core::TimerEvent&)
{
    if (m_paused)
        return;
    tick();
}

void Game::paint_event(GUI::PaintEvent& event)
{
    GUI::Painter painter(*this);
    painter.fill_rect(event.rect(), Color::Black);
    painter.draw_line({ s_game_width, 0 }, { s_game_width, s_game_height }, Gfx::Color::White, 1);

    for (auto& piece : m_pieces) {
        piece.for_each_rectangle([&](auto const& rect) {
            if (m_paused) {
                painter.fill_rect(rect, Gfx::Color::DarkGray);
            } else {
                painter.fill_rect(rect, piece.color());
            }
            if (m_debug_mode) {
                painter.draw_rect_with_thickness(rect, piece.color().inverted(), 1);
            }
        });
    }

    if (m_ghost_piece_enabled) {
        Piece ghost = m_pieces.last();

        while (!collision(ghost)) {
            ghost.move_down();
        }
        ghost.move_up();

        ghost.for_each_rectangle([&](auto const& rect) {
            painter.draw_rect_with_thickness(rect, Gfx::Color::White, 1);
        });
    }

    if (m_paused) {
        char const* const msg = "P A U S E D";
        int msg_width = font().width(msg);
        int msg_height = font().glyph_height();
        painter.draw_text({ (s_game_width / 2) - (msg_width / 2), (s_game_height / 2) - (msg_height / 2), msg_width, msg_height }, msg, Gfx::TextAlignment::Center, Color::White);
    }

    int msg_width = font().width(String::formatted("Level: {}", m_level));
    Gfx::IntRect level_rect = { (s_window_width - msg_width - 2), 2, msg_width, font().glyph_height() };
    painter.draw_text(level_rect, String::formatted("Level: {}", m_level), Gfx::TextAlignment::Center, Color::White);

    int score_width = font().width(String::formatted("Score: {}", m_score));
    Gfx::IntRect score_rect = { (s_window_width - score_width - 2), font().glyph_height() + 4, score_width, font().glyph_height() };
    painter.draw_text(score_rect, String::formatted("Score: {}", m_score), Gfx::TextAlignment::Center, Color::White);

    int lines_width = font().width(String::formatted("Lines: {}", m_total_lines_cleared));
    Gfx::IntRect lines_rect = { (s_window_width - lines_width - 2), 2 * font().glyph_height() + 6, lines_width, font().glyph_height() };
    painter.draw_text(lines_rect, String::formatted("Lines: {}", m_total_lines_cleared), Gfx::TextAlignment::Center, Color::White);
}

void Game::set_ghost(bool ghost_enabled)
{
    m_ghost_piece_enabled = ghost_enabled;
}

void Game::set_paused(bool paused)
{
    m_paused = paused;
    update(rect());
}

void Game::keydown_event(GUI::KeyEvent& event)
{
    if (m_paused)
        return;
    switch (event.key()) {
    case Key_F3:
        m_debug_mode = !m_debug_mode;
        break;
    case Key_Escape:
        GUI::Application::the()->quit();
        break;
    case Key_J:
        [[fallthrough]];
    case Key_Left:
        m_pieces.last().move_left();
        if (collision(m_pieces.last())) {
            m_pieces.last().move_right();
        } else {
            update(rect());
        }
        break;
    case Key_K:
        [[fallthrough]];
    case Key_Right:
        m_pieces.last().move_right();
        if (collision(m_pieces.last())) {
            m_pieces.last().move_left();
        } else {
            update(rect());
        }
        break;
    case Key_Down:
        m_pieces.last().move_down();
        if (collision(m_pieces.last())) {
            m_pieces.last().move_up();
            start_lock_delay();
        } else {
            update(rect());
        }
        break;
    case Key_D:
        [[fallthrough]];
    case Key_Up:
        [[fallthrough]];
    case Key_X:
        m_pieces.last().rotate_cw();
        if (collision(m_pieces.last())) {
            m_pieces.last().rotate_ccw();
        } else {
            update(rect());
        }
        break;
    case Key_S:
        [[fallthrough]];
    case Key_Z:
        m_pieces.last().rotate_ccw();
        if (collision(m_pieces.last())) {
            m_pieces.last().rotate_cw();
        } else {
            update(rect());
        }
        break;
    case Key_F:
        [[fallthrough]];
    case Key_Space:
        while (!collision(m_pieces.last())) {
            m_pieces.last().move_down();
        }
        m_pieces.last().move_up();
        update(rect());
        start_lock_delay();
        break;
    default:
        break;
    }
}

void Game::start_lock_delay()
{
    if (!m_lock_delay_timer->is_active()) {
        stop_timer();
        m_lock_delay_timer->start();
    }
}

bool Game::collision(Piece const& active_piece)
{
    // Skip the collision check for the top of the playfield to be able to rotate through the ceiling
    if (!m_playfield.contains_vertically(active_piece.bottom()) || !m_playfield.contains_horizontally(active_piece.left()) || !m_playfield.contains_horizontally(active_piece.right())) {
        return true;
    }
    for (auto& piece : m_pieces.span().slice(0, m_pieces.size() - 1)) {
        if (piece.intersects(active_piece)) {
            return true;
        }
    }
    return false;
}

Vector<Game::Line> Game::filled_lines()
{
    int width;
    Vector<Line> clear_lines;
    Line line;

    do {
        width = 0;
        for (auto& piece : m_pieces) {
            line.rectangle().for_each_intersected(piece.rects(),
                [&](auto& intersected) {
                    width += intersected.width();
                    return IterationDecision::Continue;
                });
        }

        if (width == s_game_width) {
            clear_lines.append(line);
        }

        line.move_up();
    } while (line.rectangle().y() > 0 || width != 0);
    return clear_lines;
}

void Game::increment_score(size_t const line_count)
{
    switch (line_count) {
    case 1:
        m_score += 30 * (m_level + 1);
        break;
    case 2:
        m_score += 150 * (m_level + 1);
        break;
    case 3:
        m_score += 400 * (m_level + 1);
        break;
    case 4:
        m_score += 1500 * (m_level + 1);
        break;
    default:
        break;
    }
}

void Game::clear_lines(Vector<Game::Line> const& lines)
{
    m_lines_cleared += lines.size();
    m_total_lines_cleared += lines.size();
    // Clear the lines
    for (auto const& line : lines) {
        for (auto& piece : m_pieces) {
            piece.remove_all_matching_rectangles(
                [&](auto& rect) {
                    if (line.rectangle().intersects(rect)) {
                        rect.set_height(rect.height() - s_side_length);
                    }
                    return rect.height() == 0;
                });
        }
    }

    // Clean up pieces
    m_pieces.remove_all_matching(
        [](auto& piece) {
            return piece.is_empty();
        });

    // Apply gravity
    Vector<Gfx::IntRect*> rectangles;
    for (auto const& line : lines) {
        for (auto& piece : m_pieces) {
            piece.for_each_rectangle(
                [&](auto& rect) {
                    if (rect.top() <= line.rectangle().top()) {
                        rectangles.append(&rect);
                    }
                });
        }
    }

    for (auto* r : rectangles) {
        r->set_y(r->y() + s_side_length);
    }
}

void Game::reset()
{
    m_pieces.clear();
    m_pieces.append(random_piece());
    m_level = 0;
    m_lines_cleared = 0;
    m_total_lines_cleared = 0;
    m_score = 0;
}

void Game::lock_piece()
{
    // If movement has occured during lock delay then don't lock the piece since it is potentially hovering
    m_pieces.last().move_down();
    if (collision(m_pieces.last())) {
        m_pieces.last().move_up();

        auto piece = random_piece();
        if (piece.intersects(m_pieces.last()) || collision(piece)) {
            GUI::MessageBox::show(window(), "You lose!", "BrickStacker", GUI::MessageBox::Type::Information, GUI::MessageBox::InputType::OK);
            reset();
        } else {
            auto lines = filled_lines();
            increment_score(lines.size());
            clear_lines(lines);
            if (m_lines_cleared >= 15) {
                m_lines_cleared = 0;
                if (m_level < (m_timeouts.size() - 1)) {
                    m_level++;
                }
            }
            m_pieces.append(piece);
        }
    }
    start_timer(m_timeouts[m_level]);
    update(rect());
}

void Game::tick()
{
    m_pieces.last().move_down();
    if (collision(m_pieces.last())) {
        m_pieces.last().move_up();
        start_lock_delay();
    }
    update(rect());
}

Game::Piece Game::random_piece()
{
    static Array<Piece, 7> pieces = {
        Z(),
        I(),
        S(),
        O(),
        T(),
        J(),
        L(),
    };

    return pieces[get_random<u8>() % pieces.size()];
}
}
