#pragma once
#ifndef _OPTIONX_TRADE_HANDLER_NODE_HPP_INCLUDED
#define _OPTIONX_TRADE_HANDLER_NODE_HPP_INCLUDED

namespace optionx::trading {
	
	using NodeID = size_t;
	
	class BaseTradeHandlerNode {
	public:
	
		BaseTradeHandlerNode(std::unordered_map<NodeID, std::shared_ptr<BaseTradeHandlerNode>>& handler_map) 
			: handler_map(handler_map) {};
		
		virtual ~BaseTradeHandlerNode() = default;
		
		void process_trade(NodeID handler_id, std::shared_ptr<TradeRequest> trade) {
			if (handler_map.count(handler_id)) {
				handler_map[handler_id]->process(trade);
				for (NodeID next_id : handler_connections[handler_id]) {
					process_trade(next_id, trade);
				}
			}
		}
		
	private:
		std::unordered_map<NodeID, std::shared_ptr<BaseTradeHandlerNode>>& handler_map;
		
	};

};

#endif // _OPTIONX_TRADE_HANDLER_NODE_HPP_INCLUDED