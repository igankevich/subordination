int main(int argc, char** argv) {
	App* app = static_cast<App*>(::operator new(sizeof(App)));
	bootstrap_factory = app;
	new (app) App;
	int ret = app->run(argc, argv);
	delete app;
	return ret;
}
