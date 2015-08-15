namespace factory {

	namespace bits {

		template<size_t No, class F>
		struct Apply_to {
			constexpr explicit Apply_to(F&& f): func(f) {}
			template<class Arg>
			auto operator()(const Arg& rhs) ->
				typename std::result_of<F&(decltype(std::get<No>(rhs)))>::type
			{
				return func(std::get<No>(rhs));
			}
		private:
			F&& func;
		};
	
	}

}
