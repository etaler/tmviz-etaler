#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>

#include <xtensor/xarray.hpp>
#include <xtensor/xview.hpp>

//#include <tiny_htm/tiny_htm.hpp>
//using namespace th;

#include <Etaler/Algorithms/TemporalMemory.hpp>
#include <Etaler/Encoders/Category.hpp>
#include <Etaler/Interop/Xtensor.hpp>
using namespace et;

//Global config parameters
size_t disp_cell_length = 12;
size_t disp_cell_vertical_spacing = 2;
size_t disp_cell_horizontal_spacing = 4;
size_t disp_category_spacing = 6;

intmax_t cells_per_column = 16;
intmax_t max_synapses_per_cell = 64;
intmax_t num_categories = 12;
intmax_t cell_per_catrgory = 6;

std::vector<sf::RectangleShape> makeEmptyCells(size_t y_bias = 0
	, size_t alloc_size=num_categories*cell_per_catrgory*cells_per_column
	, size_t rects_per_column=cells_per_column)
{
	std::vector<sf::RectangleShape> rects(alloc_size);
	for(size_t i=0;i<num_categories*cell_per_catrgory;i++) {
		for(size_t j=0;j<rects_per_column;j++) {
			auto rect = sf::RectangleShape(sf::Vector2f(disp_cell_length, disp_cell_length));
			rect.setPosition(i*(disp_cell_length+disp_cell_horizontal_spacing)+disp_cell_horizontal_spacing+(i/cell_per_catrgory)*disp_category_spacing+85,
				 j*(disp_cell_length+disp_cell_vertical_spacing)+disp_cell_vertical_spacing+y_bias);
			rect.setOutlineColor(sf::Color::Black);
			rect.setOutlineThickness(1);

			rects[i*rects_per_column+j] = rect;
		}
	}
	return rects;
}

void updateCells(std::vector<sf::RectangleShape>& rects, const xt::xarray<bool>& active_cells, const xt::xarray<bool>& predictive_cells)
{
	assert(rects.size() == active_cells.size());
	for(size_t i=0;i<active_cells.size();i++) {
		rects[i].setOutlineColor(sf::Color::Black);
		if(predictive_cells[i] == true && active_cells[i] == true) {
			rects[i].setFillColor(sf::Color(0.29*255, 0.707*255, 0.865*255));
			rects[i].setOutlineColor(sf::Color(252, 200, 68));
		}
		else if(predictive_cells[i] == true)
			rects[i].setFillColor(sf::Color(252, 200, 68)); //Orange
		else if(active_cells[i] == true)
			rects[i].setFillColor(sf::Color(0.29*255, 0.707*255, 0.865*255)); //Blue
		else
			rects[i].setFillColor(sf::Color::White);
	}
}

void setTextOnOff(sf::Text& text, bool on)
{
	text.setString(on?"ON":"OFF");
	text.setFillColor(on?sf::Color::Green: sf::Color::Blue);
}

auto make_tm_state_tensor(const TemporalMemory& tm)
{
	return zeros(tm.input_shape_ + tm.connections_.shape()[tm.connections_.shape().size()-2], DType::Bool);
}

int main()
{
	//Setup SFML
	sf::ContextSettings context;
	context.antialiasingLevel = 4;
	sf::RenderWindow window(sf::VideoMode(1350, 720), "tiny-htm TM visualiztion", sf::Style::Titlebar | sf::Style::Close, context);
	window.setVerticalSyncEnabled(true);

	//HTM Stuff
	//CategoryEncoder encoder(num_categories, cell_per_catrgory);
	TemporalMemory tm({(intmax_t)num_categories*cell_per_catrgory}, cells_per_column, max_synapses_per_cell);
	auto predicted_sdr = zeros({num_categories*cell_per_catrgory}, DType::Bool);

	//Visualizer states
	bool show_all_connections = false;
	bool show_active_cell_connection = false;
	bool tm_learning = true;
	bool show_predictive_connection = false;

	//Visualizer visual elements
	size_t columns_display_end = (disp_cell_vertical_spacing+disp_cell_length)*cells_per_column;
	std::vector<sf::RectangleShape> rects = makeEmptyCells();
	std::vector<sf::RectangleShape> rect_sdrs = makeEmptyCells(columns_display_end+16
		, num_categories*cell_per_catrgory, 1);
	sf::Font font;
	if(font.loadFromFile("../ProzaLibre-Light.ttf") == false)
		return 0;
	sf::Text tm_text("TM Column", font, 14);
	tm_text.setPosition(5, columns_display_end/2);
	tm_text.setFillColor(sf::Color::Black);

	sf::Text pred_text("Predictions", font, 14);
	pred_text.setPosition(5, columns_display_end+16);
	pred_text.setFillColor(sf::Color::Black);

	sf::Text learning_text("", font, 18);
	learning_text.setPosition(860, 651);
	learning_text.setFillColor(sf::Color::Black);

	sf::Text show_all_synapse_text("", font, 18);
	show_all_synapse_text.setPosition(860, 576);
	show_all_synapse_text.setFillColor(sf::Color::Black);

	sf::Text show_active_synapse_text("", font, 18);
	show_active_synapse_text.setPosition(860, 601);
	show_active_synapse_text.setFillColor(sf::Color::Black);

	sf::Text show_predictive_synapse_text("", font, 18);
	show_predictive_synapse_text.setPosition(860, 626);
	show_predictive_synapse_text.setFillColor(sf::Color::Black);

	sf::Text prediction_text("", font, 22);
	prediction_text.setPosition(85, columns_display_end+64);
	prediction_text.setFillColor(sf::Color::Black);

	std::string usage_str =
	"1-9,0,-,= - Activae column 1~12\n"
	"R - reset TM state\n"
	"I - reset TM connections and state\n"
	"C - clear unused synapses (buggy now)\n"
	"S - Show all synapses\n"
	"A - Show connections to current active cells\n"
	"P - Show connections to current prediction\n"
	"L - Enable/disable learning"
	;
	sf::Text usage_text(usage_str, font, 18);
	usage_text.setPosition(900, 475);
	usage_text.setFillColor(sf::Color::Black);

	auto prev_active = make_tm_state_tensor(tm);
	auto prev_predict = make_tm_state_tensor(tm);

	Tensor pred = make_tm_state_tensor(tm);
	Tensor active = make_tm_state_tensor(tm);
	while (window.isOpen()) {
		sf::Event event;
		while (window.pollEvent(event)) {
			if (event.type == sf::Event::Closed)
				window.close();
			else if (event.type == sf::Event::Resized)
				window.setView(sf::View(sf::FloatRect(0, 0, event.size.width, event.size.height)));
			else if (event.type == sf::Event::KeyPressed) {
				if(event.key.code == sf::Keyboard::C) {
					//tm.cells_.decaySynapse(tm.connected_permanence_);
					//tm.organizeSynapse();
				}
				else if(event.key.code == sf::Keyboard::S)
					show_all_connections = !show_all_connections;
				else if(event.key.code == sf::Keyboard::A)
					show_active_cell_connection = !show_active_cell_connection;
				else if(event.key.code == sf::Keyboard::R) {
					//tm.reset();
					prev_active = make_tm_state_tensor(tm);
					prev_predict = make_tm_state_tensor(tm);
					predicted_sdr[{}] = false;
				}
				else if(event.key.code == sf::Keyboard::L)
					tm_learning = !tm_learning;
				else if(event.key.code == sf::Keyboard::P)
					show_predictive_connection = !show_predictive_connection;
				else if(event.key.code == sf::Keyboard::I) {
					tm = TemporalMemory({(intmax_t)num_categories*cell_per_catrgory}, cells_per_column);
					predicted_sdr[{}] = false;
				}

				size_t cat = 0;
				if(event.key.code == sf::Keyboard::Num1 || event.key.code == sf::Keyboard::Numpad1)
					cat = 0;
				else if(event.key.code == sf::Keyboard::Num2 || event.key.code == sf::Keyboard::Numpad2)
					cat = 1;
				else if(event.key.code == sf::Keyboard::Num3 || event.key.code == sf::Keyboard::Numpad3)
					cat = 2;
				else if(event.key.code == sf::Keyboard::Num4 || event.key.code == sf::Keyboard::Numpad4)
					cat = 3;
				else if(event.key.code == sf::Keyboard::Num5 || event.key.code == sf::Keyboard::Numpad5)
					cat = 4;
				else if(event.key.code == sf::Keyboard::Num6 || event.key.code == sf::Keyboard::Numpad6)
					cat = 5;
				else if(event.key.code == sf::Keyboard::Num7 || event.key.code == sf::Keyboard::Numpad7)
					cat = 6;
				else if(event.key.code == sf::Keyboard::Num8 || event.key.code == sf::Keyboard::Numpad8)
					cat = 7;
				else if(event.key.code == sf::Keyboard::Num9 || event.key.code == sf::Keyboard::Numpad9)
					cat = 8;
				else if(event.key.code == sf::Keyboard::Num0 || event.key.code == sf::Keyboard::Numpad0)
					cat = 9;
				else if(event.key.code == sf::Keyboard::Dash || event.key.code == sf::Keyboard::Subtract)
					cat = 10;
				else if(event.key.code == sf::Keyboard::Equal || event.key.code == sf::Keyboard::Add)
					cat = 11;
				else
					continue;
				auto sdr = encoder::category(cat, num_categories, cell_per_catrgory);
				std::tie(pred, active) = tm.compute(sdr, prev_predict);
				predicted_sdr = pred.sum(1, DType::Bool);
				if(tm_learning)
					tm.learn(active, prev_active);
				std::tie(prev_active, prev_predict) = std::pair(active, pred);
			}
		}

		auto active_cells = to_xarray<bool>(active);
		auto predictive_cells = to_xarray<bool>(pred);
		auto tm_connections = to_xarray<int>(tm.connections());
		auto tm_permanences = to_xarray<float>(tm.permanences());

		setTextOnOff(learning_text, tm_learning);
		setTextOnOff(show_all_synapse_text, show_all_connections);
		setTextOnOff(show_active_synapse_text, show_active_cell_connection);
		setTextOnOff(show_predictive_synapse_text, show_predictive_connection);

		auto predictions = decoder::category(predicted_sdr, num_categories);
		std::string str = "Predicted: ";
		if(predictions.size() == 0)
			str += "None";
		else for(size_t i=0;i<predictions.size();i++)
			str += std::to_string(predictions[i]+1) + std::string(i == predictions.size()-1 ? "" : ", ");
		prediction_text.setString(str);

		window.clear(sf::Color::White);

		//Update output SDR view
		auto predicted_sdr_ = to_xarray<uint8_t>(predicted_sdr);
		for(size_t i=0;i<rect_sdrs.size();i++) {
			if(predicted_sdr_[i] == true)
				rect_sdrs[i].setFillColor(sf::Color(252, 200, 68));
			else
				rect_sdrs[i].setFillColor(sf::Color::White);
		}

		updateCells(rects, active_cells, predictive_cells);

		for(const auto& rect : rects)
			window.draw(rect);
		
		for(const auto& rect : rect_sdrs)
			window.draw(rect);

		//Resolve mouse on which cell and draw connections
		for(size_t i=0;i<rects.size();i++) {
			auto& rect = rects[i];
			auto range = rect.getGlobalBounds();
			auto mouse_pos = sf::Mouse::getPosition(window);
			if(rect.getGlobalBounds().contains(sf::Vector2f(mouse_pos))
				|| show_all_connections
				|| (show_active_cell_connection && active_cells[i])
				|| (show_predictive_connection && predictive_cells[i])) {

				const auto& connections = xt::view(tm_connections, i);
				const auto& permences = xt::view(tm_permanences, i);

				sf::Vector2f center(range.left+range.width/2, range.top+range.height/2);
				for(size_t j=0;j<tm.maxSynapsesPerCell();j++) {
					int idx = connections[j];
					if(idx == -1)
						break;
					const auto& target = rects[idx].getGlobalBounds();
					if(permences[j] < tm.connected_permanence_)
						continue;
					sf::Vector2f draw_to(target.left+target.width/2, target.top+target.height/2);

					sf::VertexArray lines(sf::LinesStrip, 2);
					lines[0].position = center;
					lines[1].position = draw_to;
					lines[0].color = sf::Color::Blue;
					lines[1].color = sf::Color::Green;
					window.draw(lines);
				}

			}
		}

		window.draw(tm_text);
		window.draw(pred_text);
		window.draw(usage_text);
		window.draw(learning_text);
		window.draw(show_all_synapse_text);
		window.draw(show_active_synapse_text);
		window.draw(show_predictive_synapse_text);
		window.draw(prediction_text);


		window.display();
	}
	
	return 0;
}
