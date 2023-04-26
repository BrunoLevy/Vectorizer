-- Lua (keep this comment, it is an indication for editor's 'run' command)
-- script for Graphite (https://github.com/BrunoLevy/GraphiteThree)
-- displays an "animation" where each object of the scene graph is a frame
-- usage: graphite PATHS/*.obj animate.lua

camera.draw_selected_only=true
scene_graph.current().shader.edges_style='true; 1 0 1 1; 1'
scene_graph.current().shader.vertices_style='true; 1 0 1 1; 1' 
scene_graph.scene_graph_shader_manager.apply_to_scene_graph()

function sleep(t)
   local ntime = os.clock()+t/10
   repeat until os.clock() > ntime
end

for i = 0,scene_graph.nb_children-1,1 do
   scene_graph.current_object = scene_graph.ith_child(i).name
   main.draw()
   sleep(0.1)
end




