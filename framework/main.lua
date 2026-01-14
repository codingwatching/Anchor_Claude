

require('anchor')

an:layer('game')
an:font('main', 'assets/LanaPixel.ttf', 11)
an:image('smile', 'assets/slight_smile.png')

an.angle = 0;return 

an:action(function(self, dt)
self.angle = self.angle + (dt * 2)local game = 
an.layers.game
game:rectangle(80, 80, 50, 50, rgba(255, 0, 0, 255))
game:circle(400, 80, 25, rgba(0, 255, 0, 255))
game:push(240, 135, self.angle, 0.1, 0.1)
game:image(an.images.smile, 0, 0)
game:pop()
game:text("Hello!", an.fonts.main, 240, 220, rgba(255, 255, 255, 255))return 
game:draw()end)